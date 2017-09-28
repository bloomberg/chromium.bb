// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/simple_url_loader.h"

#include <stdint.h>

#include <algorithm>
#include <limits>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/task_scheduler/post_task.h"
#include "base/task_scheduler/task_traits.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "content/public/common/resource_request.h"
#include "content/public/common/resource_response.h"
#include "content/public/common/url_loader.mojom.h"
#include "content/public/common/url_loader_factory.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace content {

namespace {

class SimpleURLLoaderImpl;

// BodyHandler is an abstract class from which classes for a specific use case
// (e.g. read response to a string, write it to a file) will derive. Those
// clases may use BodyReader to handle reading data from the body pipe.

// Utility class to drive the pipe reading a response body. Can be created on
// one thread and then used to read data on another.
class BodyReader {
 public:
  class Delegate {
   public:
    Delegate() {}

    // The specified amount of data was read from the pipe. The BodyReader must
    // not be deleted during this callback. The Delegate should return net::OK
    // to continue reading, or a value indicating an error if the pipe should be
    // closed.
    virtual net::Error OnDataRead(uint32_t length, const char* data) = 0;

    // Called when the pipe is closed by the remote size, the size limit is
    // reached, or OnDataRead returned an error. |error| is net::OK if the
    // pipe was closed, or an error value otherwise. It is safe to delete the
    // BodyReader during this callback.
    virtual void OnDone(net::Error error, int64_t total_bytes) = 0;

   protected:
    virtual ~Delegate() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  BodyReader(Delegate* delegate, int64_t max_body_size)
      : delegate_(delegate), max_body_size_(max_body_size) {
    DCHECK_GE(max_body_size_, 0);
  }

  // Makes the reader start reading from |body_data_pipe|. May only be called
  // once. The reader will continuously to try to read from the pipe (without
  // blocking the thread), calling OnDataRead as data is read, until one of the
  // following happens:
  // * The size limit is reached.
  // * OnDataRead returns an error.
  // * The BodyReader is deleted.
  void Start(mojo::ScopedDataPipeConsumerHandle body_data_pipe) {
    DCHECK(!body_data_pipe_.is_valid());
    body_data_pipe_ = std::move(body_data_pipe);
    handle_watcher_ = std::make_unique<mojo::SimpleWatcher>(
        FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL);
    handle_watcher_->Watch(
        body_data_pipe_.get(),
        MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
        MOJO_WATCH_CONDITION_SATISFIED,
        base::BindRepeating(&BodyReader::MojoReadyCallback,
                            base::Unretained(this)));
    ReadData();
  }

 private:
  void MojoReadyCallback(MojoResult result,
                         const mojo::HandleSignalsState& state) {
    ReadData();
  }

  // Reads as much data as possible from |body_data_pipe_|, copying it to
  // |body_|. Arms |handle_watcher_| when data is not currently available.
  void ReadData() {
    while (true) {
      const void* body_data;
      uint32_t read_size;
      MojoResult result = body_data_pipe_->BeginReadData(
          &body_data, &read_size, MOJO_READ_DATA_FLAG_NONE);
      if (result == MOJO_RESULT_SHOULD_WAIT) {
        handle_watcher_->ArmOrNotify();
        return;
      }

      // If the pipe was closed, unclear if it was an error or success. Notify
      // the consumer of how much data was received.
      if (result != MOJO_RESULT_OK) {
        // The only error other than MOJO_RESULT_SHOULD_WAIT this should fail
        // with is MOJO_RESULT_FAILED_PRECONDITION, in the case the pipe was
        // closed.
        DCHECK_EQ(MOJO_RESULT_FAILED_PRECONDITION, result);
        ClosePipe();
        delegate_->OnDone(net::OK, total_bytes_read_);
        return;
      }

      // Check size against the limit.
      uint32_t copy_size = read_size;
      if (static_cast<int64_t>(copy_size) > max_body_size_ - total_bytes_read_)
        copy_size = max_body_size_ - total_bytes_read_;

      total_bytes_read_ += copy_size;
      net::Error error =
          delegate_->OnDataRead(copy_size, static_cast<const char*>(body_data));
      body_data_pipe_->EndReadData(read_size);
      // Handling reads asynchronously is currently not supported.
      DCHECK_NE(error, net::ERR_IO_PENDING);
      if (error == net::OK && copy_size < read_size)
        error = net::ERR_INSUFFICIENT_RESOURCES;

      if (error) {
        ClosePipe();
        delegate_->OnDone(error, total_bytes_read_);
        return;
      }
    }
  }

  // Frees Mojo resources and prevents any more Mojo messages from arriving.
  void ClosePipe() {
    handle_watcher_.reset();
    body_data_pipe_.reset();
  }

  mojo::ScopedDataPipeConsumerHandle body_data_pipe_;
  std::unique_ptr<mojo::SimpleWatcher> handle_watcher_;

  Delegate* const delegate_;

  const int64_t max_body_size_;
  int64_t total_bytes_read_ = 0;

  DISALLOW_COPY_AND_ASSIGN(BodyReader);
};

// Class to drive the pipe for reading the body, handle the results of the body
// read as appropriate, and invoke the consumer's callback to notify it of
// request completion. Implementations typically use a BodyReader to to manage
// reads from the body data pipe.
class BodyHandler {
 public:
  // A raw pointer is safe, since |simple_url_loader| owns the BodyHandler.
  explicit BodyHandler(SimpleURLLoaderImpl* simple_url_loader)
      : simple_url_loader_(simple_url_loader) {}
  virtual ~BodyHandler() {}

  // Called by SimpleURLLoader with the data pipe received from the URLLoader.
  // The BodyHandler is responsible for reading from it and monitoring it for
  // closure. Should call SimpleURLLoaderImpl::OnBodyHandlerDone(), once either
  // when the body pipe is closed or when an error occurs, like a write to a
  // file fails.
  virtual void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body_data_pipe) = 0;

  // Called by SimpleURLLoader. Notifies the SimpleURLLoader's consumer that the
  // request has completed, either successfully or with an error. May be invoked
  // before OnStartLoadingResponseBody(), or before the BodyHandler has notified
  // SimplerURLLoader of completion or an error. Once this is called, the
  // BodyHandler must not invoke any of SimpleURLLoaderImpl's callbacks. If
  // |destroy_results| is true, any received data should be destroyed instead of
  // being sent to the consumer.
  virtual void NotifyConsumerOfCompletion(bool destroy_results) = 0;

 protected:
  SimpleURLLoaderImpl* simple_url_loader() { return simple_url_loader_; }

 private:
  SimpleURLLoaderImpl* const simple_url_loader_;

  DISALLOW_COPY_AND_ASSIGN(BodyHandler);
};

// BodyHandler implementation for consuming the response as a string.
class SaveToStringBodyHandler : public BodyHandler,
                                public BodyReader::Delegate {
 public:
  SaveToStringBodyHandler(
      SimpleURLLoaderImpl* simple_url_loader,
      SimpleURLLoader::BodyAsStringCallback body_as_string_callback,
      int64_t max_body_size)
      : BodyHandler(simple_url_loader),
        body_as_string_callback_(std::move(body_as_string_callback)),
        body_reader_(std::make_unique<BodyReader>(this, max_body_size)) {}

  ~SaveToStringBodyHandler() override {}

  // BodyHandler implementation:
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body_data_pipe) override;
  void NotifyConsumerOfCompletion(bool destroy_results) override;

 private:
  // BodyReader::Delegate implementation.
  net::Error OnDataRead(uint32_t length, const char* data) override;
  void OnDone(net::Error error, int64_t total_bytes) override;

  std::unique_ptr<std::string> body_;
  SimpleURLLoader::BodyAsStringCallback body_as_string_callback_;

  std::unique_ptr<BodyReader> body_reader_;

  DISALLOW_COPY_AND_ASSIGN(SaveToStringBodyHandler);
};

// BodyHandler implementation for saving the response to a file
class SaveToFileBodyHandler : public BodyHandler {
 public:
  // |net_priority| is the priority from the ResourceRequest, and is used to
  // determine the TaskPriority of the sequence used to read from the response
  // body and write to the file.
  SaveToFileBodyHandler(SimpleURLLoaderImpl* simple_url_loader,
                        SimpleURLLoader::DownloadToFileCompleteCallback
                            download_to_file_complete_callback,
                        const base::FilePath& path,
                        uint64_t max_body_size,
                        net::RequestPriority request_priority)
      : BodyHandler(simple_url_loader),
        download_to_file_complete_callback_(
            std::move(download_to_file_complete_callback)),
        weak_ptr_factory_(this) {
    // Choose the TaskPriority based on the net request priority.
    // TODO(mmenke): Can something better be done here?
    base::TaskPriority task_priority;
    if (request_priority >= net::MEDIUM) {
      task_priority = base::TaskPriority::USER_BLOCKING;
    } else if (request_priority >= net::LOW) {
      task_priority = base::TaskPriority::USER_VISIBLE;
    } else {
      task_priority = base::TaskPriority::BACKGROUND;
    }

    // Can only do this after initializing the WeakPtrFactory.
    file_writer_ =
        std::make_unique<FileWriter>(path, max_body_size, task_priority);
  }

  ~SaveToFileBodyHandler() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (file_writer_)
      FileWriter::Destroy(std::move(file_writer_), base::OnceClosure());
  }

  // BodyHandler implementation:
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body_data_pipe) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    file_writer_->StartWriting(std::move(body_data_pipe),
                               base::BindOnce(&SaveToFileBodyHandler::OnDone,
                                              weak_ptr_factory_.GetWeakPtr()));
  }

  void NotifyConsumerOfCompletion(bool destroy_results) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(!download_to_file_complete_callback_.is_null());

    if (destroy_results) {
      // Prevent the FileWriter from calling OnDone().
      weak_ptr_factory_.InvalidateWeakPtrs();

      // To avoid any issues if the consumer tries to re-download a file to the
      // same location, don't invoke the callback until any partially downloaded
      // file has been destroyed.
      FileWriter::Destroy(
          std::move(file_writer_),
          base::Bind(&SaveToFileBodyHandler::InvokeCallbackAsynchronously,
                     weak_ptr_factory_.GetWeakPtr()));
      return;
    }

    file_writer_->ReleaseFile();
    std::move(download_to_file_complete_callback_).Run(path_);
  }

 private:
  // Class to read from a mojo::ScopedDataPipeConsumerHandle and write the
  // contents to a file. Does all reading and writing on a separate file
  // SequencedTaskRunner. All public methods except the destructor are called on
  // BodyHandler's TaskRunner.  All private methods and the destructor are run
  // on the file TaskRunner.
  //
  // Destroys the file that it's saving to on destruction, unless ReleaseFile()
  // is called first, so on cancellation and errors, the default behavior is to
  // clean up after itself.
  //
  // FileWriter is owned by the SaveToFileBodyHandler and destroyed by a task
  // moving its unique_ptr to the |file_writer_task_runner_|. As a result, tasks
  // posted to |file_writer_task_runner_| can always use base::Unretained. Tasks
  // posted the other way, however, require the SaveToFileBodyHandler to use
  // WeakPtrs, since the SaveToFileBodyHandler can be destroyed at any time.
  class FileWriter : public BodyReader::Delegate {
   public:
    using OnDoneCallback = base::OnceCallback<void(net::Error error,
                                                   int64_t total_bytes,
                                                   const base::FilePath& path)>;

    explicit FileWriter(const base::FilePath& path,
                        int64_t max_body_size,
                        base::TaskPriority priority)
        : body_handler_task_runner_(base::SequencedTaskRunnerHandle::Get()),
          file_writer_task_runner_(base::CreateSequencedTaskRunnerWithTraits(
              {base::MayBlock(), priority,
               base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
          path_(path),
          body_reader_(std::make_unique<BodyReader>(this, max_body_size)) {
      DCHECK(body_handler_task_runner_->RunsTasksInCurrentSequence());
    }

    // Starts reading from |body_data_pipe| and writing to the file.
    void StartWriting(mojo::ScopedDataPipeConsumerHandle body_data_pipe,
                      OnDoneCallback on_done_callback) {
      DCHECK(body_handler_task_runner_->RunsTasksInCurrentSequence());
      file_writer_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&FileWriter::StartWritingOnFileSequence,
                         base::Unretained(this), std::move(body_data_pipe),
                         std::move(on_done_callback)));
    }

    // Releases ownership of the downloaded file. If not called before
    // destruction, file will automatically be destroyed in the destructor.
    void ReleaseFile() {
      DCHECK(body_handler_task_runner_->RunsTasksInCurrentSequence());
      file_writer_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&FileWriter::ReleaseFileOnFileSequence,
                                    base::Unretained(this)));
    }

    // Destroys the FileWriter on the file TaskRunner.
    //
    // If |on_destroyed_closure| is non-null, it will be invoked on the caller's
    // task runner once the FileWriter has been destroyed.
    static void Destroy(std::unique_ptr<FileWriter> file_writer,
                        base::OnceClosure on_destroyed_closure) {
      DCHECK(
          file_writer->body_handler_task_runner_->RunsTasksInCurrentSequence());
      // Have to stash this pointer before posting a task, since |file_writer|
      // is bound to the callback that's posted to the TaskRunner.
      base::SequencedTaskRunner* task_runner =
          file_writer->file_writer_task_runner_.get();

      task_runner->PostTask(FROM_HERE,
                            base::BindOnce(&FileWriter::DestroyOnFileSequence,
                                           std::move(file_writer),
                                           std::move(on_destroyed_closure)));
    }

    // Destructor is only public so the consumer can keep it in a unique_ptr.
    // Class must be destroyed by using Destroy().
    ~FileWriter() override {
      DCHECK(file_writer_task_runner_->RunsTasksInCurrentSequence());
      file_.Close();

      if (owns_file_) {
        DCHECK(!path_.empty());
        base::DeleteFile(path_, false /* recursive */);
      }
    }

   private:
    void StartWritingOnFileSequence(
        mojo::ScopedDataPipeConsumerHandle body_data_pipe,
        OnDoneCallback on_done_callback) {
      DCHECK(file_writer_task_runner_->RunsTasksInCurrentSequence());
      DCHECK(!file_.IsValid());

      // Try to create the file.
      file_.Initialize(path_,
                       base::File::FLAG_WRITE | base::File::FLAG_CREATE_ALWAYS);
      if (!file_.IsValid()) {
        body_handler_task_runner_->PostTask(
            FROM_HERE, base::BindOnce(std::move(on_done_callback),
                                      net::MapSystemError(
                                          logging::GetLastSystemErrorCode()),
                                      0, base::FilePath()));
        return;
      }

      on_done_callback_ = std::move(on_done_callback);
      owns_file_ = true;
      body_reader_->Start(std::move(body_data_pipe));
    }

    // BodyReader::Delegate implementation:
    net::Error OnDataRead(uint32_t length, const char* data) override {
      DCHECK(file_writer_task_runner_->RunsTasksInCurrentSequence());
      while (length > 0) {
        int written = file_.WriteAtCurrentPos(
            data, std::min(length, static_cast<uint32_t>(
                                       std::numeric_limits<int>::max())));
        if (written < 0)
          return net::MapSystemError(logging::GetLastSystemErrorCode());
        length -= written;
        data += written;
      }

      return net::OK;
    }

    void OnDone(net::Error error, int64_t total_bytes) override {
      DCHECK(file_writer_task_runner_->RunsTasksInCurrentSequence());
      // This should only be called if the file was successfully created.
      DCHECK(file_.IsValid());

      // Close the file so that there's no ownership contention when the
      // consumer uses it.
      file_.Close();

      body_handler_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(std::move(on_done_callback_), error,
                                    total_bytes, path_));
    }

    // Closes the file, so it won't be deleted on destruction.
    void ReleaseFileOnFileSequence() {
      DCHECK(file_writer_task_runner_->RunsTasksInCurrentSequence());
      owns_file_ = false;
    }

    static void DestroyOnFileSequence(std::unique_ptr<FileWriter> file_writer,
                                      base::OnceClosure on_destroyed_closure) {
      DCHECK(
          file_writer->file_writer_task_runner_->RunsTasksInCurrentSequence());

      // Need to grab this before deleting |file_writer|.
      scoped_refptr<base::SequencedTaskRunner> body_handler_task_runner =
          file_writer->body_handler_task_runner_;

      // Need to delete |FileWriter| before posting a task to invoke the
      // callback.
      file_writer.reset();

      if (on_destroyed_closure)
        body_handler_task_runner->PostTask(FROM_HERE,
                                           std::move(on_destroyed_closure));
    }

    // These are set on cosntruction and accessed on both task runners.
    const scoped_refptr<base::SequencedTaskRunner> body_handler_task_runner_;
    const scoped_refptr<base::SequencedTaskRunner> file_writer_task_runner_;

    // After construction, all other values are only read and written on the
    // |file_writer_task_runner_|.

    base::FilePath path_;
    // File being downloaded to. Created just before reading from the data pipe.
    base::File file_;

    OnDoneCallback on_done_callback_;

    std::unique_ptr<BodyReader> body_reader_;

    // If true, destroys the file in the destructor. Set to true once the file
    // is created.
    bool owns_file_ = false;

    DISALLOW_COPY_AND_ASSIGN(FileWriter);
  };

  // Called by FileWriter::Destroy after deleting a partially downloaded file.
  void InvokeCallbackAsynchronously() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    std::move(download_to_file_complete_callback_).Run(base::FilePath());
  }

  void OnDone(net::Error error,
              int64_t total_bytes,
              const base::FilePath& path);

  // Path of the file. Set in OnDone().
  base::FilePath path_;

  SimpleURLLoader::DownloadToFileCompleteCallback
      download_to_file_complete_callback_;

  std::unique_ptr<FileWriter> file_writer_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SaveToFileBodyHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SaveToFileBodyHandler);
};

class SimpleURLLoaderImpl : public SimpleURLLoader,
                            public mojom::URLLoaderClient {
 public:
  SimpleURLLoaderImpl();
  ~SimpleURLLoaderImpl() override;

  // SimpleURLLoader implementation.
  void DownloadToString(const ResourceRequest& resource_request,
                        mojom::URLLoaderFactory* url_loader_factory,
                        const net::NetworkTrafficAnnotationTag& annotation_tag,
                        BodyAsStringCallback body_as_string_callback,
                        size_t max_body_size) override;
  void DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      const ResourceRequest& resource_request,
      mojom::URLLoaderFactory* url_loader_factory,
      const net::NetworkTrafficAnnotationTag& annotation_tag,
      BodyAsStringCallback body_as_string_callback) override;
  void DownloadToFile(
      const ResourceRequest& resource_request,
      mojom::URLLoaderFactory* url_loader_factory,
      const net::NetworkTrafficAnnotationTag& annotation_tag,
      DownloadToFileCompleteCallback download_to_file_complete_callback,
      const base::FilePath& file_path,
      int64_t max_body_size) override;
  void SetAllowPartialResults(bool allow_partial_results) override;
  void SetAllowHttpErrorResults(bool allow_http_error_results) override;
  int NetError() const override;
  const ResourceResponseHead* ResponseInfo() const override;

  // Called by BodyHandler when the BodyHandler body handler is done. If |error|
  // is not net::OK, some error occurred reading or consuming the body. If it is
  // net::OK, the pipe was closed and all data received was successfully
  // handled. This could indicate an error, concellation, or completion. To
  // determine which case this is, the size will also be compared to the size
  // reported in ResourceRequestCompletionStatus(), if
  // ResourceRequestCompletionStatus indicates a success.
  void OnBodyHandlerDone(net::Error error, int64_t received_body_size);

  // Finished the request with the provided error code, after freeing Mojo
  // resources. Closes any open pipes, so no URLLoader or BodyHandlers callbacks
  // will be invoked after this is called.
  void FinishWithResult(int net_error);

 private:
  void StartInternal(const ResourceRequest& resource_request,
                     mojom::URLLoaderFactory* url_loader_factory,
                     const net::NetworkTrafficAnnotationTag& annotation_tag);

  // mojom::URLLoaderClient implementation;
  void OnReceiveResponse(const ResourceResponseHead& response_head,
                         const base::Optional<net::SSLInfo>& ssl_info,
                         mojom::DownloadedTempFilePtr downloaded_file) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         const ResourceResponseHead& response_head) override;
  void OnDataDownloaded(int64_t data_length, int64_t encoded_length) override;
  void OnReceiveCachedMetadata(const std::vector<uint8_t>& data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnComplete(const ResourceRequestCompletionStatus& status) override;

  // Bound to the URLLoaderClient message pipe (|client_binding_|) via
  // set_connection_error_handler.
  void OnConnectionError();

  // Completes the request by calling FinishWithResult() if OnComplete() was
  // called and either no body pipe was ever received, or the body pipe was
  // closed.
  void MaybeComplete();

  bool allow_partial_results_ = false;
  bool allow_http_error_results_ = false;

  mojom::URLLoaderPtr url_loader_;
  mojo::Binding<mojom::URLLoaderClient> client_binding_;
  std::unique_ptr<BodyHandler> body_handler_;

  bool request_completed_ = false;
  // The expected total size of the body, taken from
  // ResourceRequestCompletionStatus.
  int64_t expected_body_size_ = 0;

  bool body_started_ = false;
  bool body_completed_ = false;
  // Final size of the body. Set once the body's Mojo pipe has been closed.
  int64_t received_body_size_ = 0;

  // Set to true when FinishWithResult() is called. Once that happens, the
  // consumer is informed of completion, and both pipes are closed.
  bool finished_ = false;

  // Result of the request.
  int net_error_ = net::ERR_IO_PENDING;

  std::unique_ptr<ResourceResponseHead> response_info_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(SimpleURLLoaderImpl);
};

void SaveToStringBodyHandler::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body_data_pipe) {
  DCHECK(!body_);

  body_ = base::MakeUnique<std::string>();
  body_reader_->Start(std::move(body_data_pipe));
}

net::Error SaveToStringBodyHandler::OnDataRead(uint32_t length,
                                               const char* data) {
  body_->append(data, length);
  return net::OK;
}

void SaveToStringBodyHandler::OnDone(net::Error error, int64_t total_bytes) {
  DCHECK_EQ(body_->size(), static_cast<size_t>(total_bytes));
  simple_url_loader()->OnBodyHandlerDone(error, total_bytes);
}

void SaveToStringBodyHandler::NotifyConsumerOfCompletion(bool destroy_results) {
  body_reader_.reset();
  if (destroy_results)
    body_.reset();

  std::move(body_as_string_callback_).Run(std::move(body_));
}

void SaveToFileBodyHandler::OnDone(net::Error error,
                                   int64_t total_bytes,
                                   const base::FilePath& path) {
  path_ = path;
  simple_url_loader()->OnBodyHandlerDone(error, total_bytes);
}

SimpleURLLoaderImpl::SimpleURLLoaderImpl() : client_binding_(this) {
  // Allow creation and use on different threads.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

SimpleURLLoaderImpl::~SimpleURLLoaderImpl() {}

void SimpleURLLoaderImpl::DownloadToString(
    const ResourceRequest& resource_request,
    mojom::URLLoaderFactory* url_loader_factory,
    const net::NetworkTrafficAnnotationTag& annotation_tag,
    BodyAsStringCallback body_as_string_callback,
    size_t max_body_size) {
  DCHECK_LE(max_body_size, kMaxBoundedStringDownloadSize);
  body_handler_ = base::MakeUnique<SaveToStringBodyHandler>(
      this, std::move(body_as_string_callback), max_body_size);
  StartInternal(resource_request, url_loader_factory, annotation_tag);
}

void SimpleURLLoaderImpl::DownloadToStringOfUnboundedSizeUntilCrashAndDie(
    const ResourceRequest& resource_request,
    mojom::URLLoaderFactory* url_loader_factory,
    const net::NetworkTrafficAnnotationTag& annotation_tag,
    BodyAsStringCallback body_as_string_callback) {
  body_handler_ = base::MakeUnique<SaveToStringBodyHandler>(
      this, std::move(body_as_string_callback),
      // int64_t because ResourceRequestCompletionStatus::decoded_body_length is
      // an int64_t, not a size_t.
      std::numeric_limits<int64_t>::max());
  StartInternal(resource_request, url_loader_factory, annotation_tag);
}

void SimpleURLLoaderImpl::DownloadToFile(
    const ResourceRequest& resource_request,
    mojom::URLLoaderFactory* url_loader_factory,
    const net::NetworkTrafficAnnotationTag& annotation_tag,
    DownloadToFileCompleteCallback download_to_file_complete_callback,
    const base::FilePath& file_path,
    int64_t max_body_size) {
  body_handler_ = std::make_unique<SaveToFileBodyHandler>(
      this, std::move(download_to_file_complete_callback), file_path,
      max_body_size, resource_request.priority);
  StartInternal(resource_request, url_loader_factory, annotation_tag);
}

void SimpleURLLoaderImpl::SetAllowPartialResults(bool allow_partial_results) {
  // Simplest way to check if a request has not yet been started.
  DCHECK(!body_handler_);
  allow_partial_results_ = allow_partial_results;
}

void SimpleURLLoaderImpl::SetAllowHttpErrorResults(
    bool allow_http_error_results) {
  // Simplest way to check if a request has not yet been started.
  DCHECK(!body_handler_);
  allow_http_error_results_ = allow_http_error_results;
}

int SimpleURLLoaderImpl::NetError() const {
  // Should only be called once the request is compelete.
  DCHECK(finished_);
  DCHECK_NE(net::ERR_IO_PENDING, net_error_);
  return net_error_;
}

const ResourceResponseHead* SimpleURLLoaderImpl::ResponseInfo() const {
  // Should only be called once the request is compelete.
  DCHECK(finished_);
  return response_info_.get();
}

void SimpleURLLoaderImpl::OnBodyHandlerDone(net::Error error,
                                            int64_t received_body_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(body_started_);
  DCHECK(!body_completed_);

  // If there's an error, fail request and report it immediately.
  if (error != net::OK) {
    FinishWithResult(error);
    return;
  }

  // Otherwise, need to wait until the URLRequestClient pipe receives a complete
  // message or is closed, to determine if the entire body was received.
  body_completed_ = true;
  received_body_size_ = received_body_size;
  MaybeComplete();
}

void SimpleURLLoaderImpl::FinishWithResult(int net_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!finished_);

  client_binding_.Close();
  url_loader_.reset();

  finished_ = true;
  net_error_ = net_error;
  // If it's a partial download or an error was received, erase the body.
  bool destroy_results = net_error_ != net::OK && !allow_partial_results_;
  body_handler_->NotifyConsumerOfCompletion(destroy_results);
}

void SimpleURLLoaderImpl::StartInternal(
    const ResourceRequest& resource_request,
    mojom::URLLoaderFactory* url_loader_factory,
    const net::NetworkTrafficAnnotationTag& annotation_tag) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // It's illegal to use a single SimpleURLLoaderImpl to make multiple requests.
  DCHECK(!finished_);
  DCHECK(!url_loader_);
  DCHECK(!body_started_);

  mojom::URLLoaderClientPtr client_ptr;
  client_binding_.Bind(mojo::MakeRequest(&client_ptr));
  client_binding_.set_connection_error_handler(base::BindOnce(
      &SimpleURLLoaderImpl::OnConnectionError, base::Unretained(this)));
  url_loader_factory->CreateLoaderAndStart(
      mojo::MakeRequest(&url_loader_), 0 /* routing_id */, 0 /* request_id */,
      0 /* options */, resource_request, std::move(client_ptr),
      net::MutableNetworkTrafficAnnotationTag(annotation_tag));
}

void SimpleURLLoaderImpl::OnReceiveResponse(
    const ResourceResponseHead& response_head,
    const base::Optional<net::SSLInfo>& ssl_info,
    mojom::DownloadedTempFilePtr downloaded_file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (response_info_) {
    // The final headers have already been received, so the URLLoader is
    // violating the API contract.
    FinishWithResult(net::ERR_UNEXPECTED);
    return;
  }

  response_info_ = base::MakeUnique<ResourceResponseHead>(response_head);
  if (!allow_http_error_results_ && response_head.headers &&
      response_head.headers->response_code() / 100 != 2) {
    FinishWithResult(net::ERR_FAILED);
  }
}

void SimpleURLLoaderImpl::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    const ResourceResponseHead& response_head) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (response_info_) {
    // If the headers have already been received, the URLLoader is violating the
    // API contract.
    FinishWithResult(net::ERR_UNEXPECTED);
    return;
  }
  url_loader_->FollowRedirect();
}

void SimpleURLLoaderImpl::OnDataDownloaded(int64_t data_length,
                                           int64_t encoded_length) {
  NOTIMPLEMENTED();
}

void SimpleURLLoaderImpl::OnReceiveCachedMetadata(
    const std::vector<uint8_t>& data) {
  NOTREACHED();
}

void SimpleURLLoaderImpl::OnTransferSizeUpdated(int32_t transfer_size_diff) {}

void SimpleURLLoaderImpl::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback ack_callback) {}

void SimpleURLLoaderImpl::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (body_started_ || !response_info_) {
    // If this was already called, or the headers have not yet been received,
    // the URLLoader is violating the API contract.
    FinishWithResult(net::ERR_UNEXPECTED);
    return;
  }
  body_started_ = true;
  body_handler_->OnStartLoadingResponseBody(std::move(body));
}

void SimpleURLLoaderImpl::OnComplete(
    const ResourceRequestCompletionStatus& status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Request should not have been completed yet.
  DCHECK(!finished_);
  DCHECK(!request_completed_);

  // Close pipes to ignore any subsequent close notification.
  client_binding_.Close();
  url_loader_.reset();

  request_completed_ = true;
  expected_body_size_ = status.decoded_body_length;
  net_error_ = status.error_code;
  // If |status| indicates success, but the body pipe was never received, the
  // URLLoader is violating the API contract.
  if (net_error_ == net::OK && !body_started_)
    net_error_ = net::ERR_UNEXPECTED;

  MaybeComplete();
}

void SimpleURLLoaderImpl::OnConnectionError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // |this| closes the pipe to the URLLoader in OnComplete(), so this method
  // being called indicates the pipe was closed before completion, most likely
  // due to peer death, or peer not calling OnComplete() on cancellation.

  // Request should not have been completed yet.
  DCHECK(!finished_);
  DCHECK(!request_completed_);
  DCHECK_EQ(net::ERR_IO_PENDING, net_error_);

  request_completed_ = true;
  net_error_ = net::ERR_FAILED;
  // Wait to receive any pending data on the data pipe before reporting the
  // failure.
  MaybeComplete();
}

void SimpleURLLoaderImpl::MaybeComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Request should not have completed yet.
  DCHECK(!finished_);

  // Make sure the URLLoader's pipe has been closed.
  if (!request_completed_)
    return;

  // Make sure the body pipe either was never opened or has been closed. Even if
  // the request failed, if allow_partial_results_ is true, may still be able to
  // read more data.
  if (body_started_ && !body_completed_)
    return;

  // When OnCompleted sees a success result, still need to report an error if
  // the size isn't what was expected.
  if (net_error_ == net::OK && expected_body_size_ != received_body_size_) {
    if (expected_body_size_ > received_body_size_) {
      // The body pipe was closed before it received the entire body.
      net_error_ = net::ERR_FAILED;
    } else {
      // The caller provided more data through the pipe than it reported in
      // ResourceRequestCompletionStatus, so the URLLoader is violating the API
      // contract. Just fail the request.
      net_error_ = net::ERR_UNEXPECTED;
    }
  }

  FinishWithResult(net_error_);
}

}  // namespace

std::unique_ptr<SimpleURLLoader> SimpleURLLoader::Create() {
  return base::MakeUnique<SimpleURLLoaderImpl>();
}

SimpleURLLoader::~SimpleURLLoader() {}

SimpleURLLoader::SimpleURLLoader() {}

}  // namespace content
