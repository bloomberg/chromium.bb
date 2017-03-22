// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/ranker_model_loader.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/profiler/scoped_tracker.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/translate/core/browser/proto/ranker_model.pb.h"
#include "components/translate/core/browser/ranker_model.h"
#include "components/translate/core/browser/translate_url_fetcher.h"
#include "url/gurl.h"

namespace translate {
namespace {

using chrome_intelligence::RankerModel;
using chrome_intelligence::RankerModelProto;

constexpr int kUrlFetcherId = 2;

// The maximum number of model download attempts to make. Download may fail
// due to server error or network availability issues.
constexpr int kMaxRetryOn5xx = 8;

// The minimum duration, in minutes, between download attempts.
constexpr int kMinRetryDelayMins = 3;

// Suffixes for the various histograms produced by the backend.
const char kWriteTimerHistogram[] = ".Timer.WriteModel";
const char kReadTimerHistogram[] = ".Timer.ReadModel";
const char kDownloadTimerHistogram[] = ".Timer.DownloadModel";
const char kParsetimerHistogram[] = ".Timer.ParseModel";
const char kModelStatusHistogram[] = ".Model.Status";

// A helper class to produce a scoped timer histogram that supports using a
// non-static-const name.
class MyScopedHistogramTimer {
 public:
  MyScopedHistogramTimer(const base::StringPiece& name)
      : name_(name.begin(), name.end()), start_(base::TimeTicks::Now()) {}

  ~MyScopedHistogramTimer() {
    base::TimeDelta duration = base::TimeTicks::Now() - start_;
    base::HistogramBase* counter = base::Histogram::FactoryTimeGet(
        name_, base::TimeDelta::FromMilliseconds(10),
        base::TimeDelta::FromMilliseconds(200000), 100,
        base::HistogramBase::kUmaTargetedHistogramFlag);
    if (counter)
      counter->AddTime(duration);
  }

 private:
  const std::string name_;
  const base::TimeTicks start_;

  DISALLOW_COPY_AND_ASSIGN(MyScopedHistogramTimer);
};

}  // namespace

// =============================================================================
// RankerModelLoader::Backend

class RankerModelLoader::Backend {
 public:
  // An internal version of RankerModelLoader::OnModelAvailableCallback that
  // bundles calling the real callback with a notification of whether or not
  // tha backend is finished.
  using InternalOnModelAvailableCallback =
      base::Callback<void(std::unique_ptr<RankerModel>, bool)>;

  Backend(const ValidateModelCallback& validate_model_cb,
          const InternalOnModelAvailableCallback& on_model_available_cb,
          const base::FilePath& model_path,
          const GURL& model_url,
          const std::string& uma_prefix);
  ~Backend();

  // Reads the model from |model_path_|.
  void LoadFromFile();

  // Reads the model from |model_url_|.
  void AsyncLoadFromURL();

 private:
  // Log and return the result of loading a model to UMA.
  RankerModelStatus ReportModelStatus(RankerModelStatus model_status);

  // Constructs a model from the given |data|.
  std::unique_ptr<chrome_intelligence::RankerModel> CreateModel(
      const std::string& data);

  // Accepts downloaded model data. This signature is mandated by the callback
  // defined by TransalteURLFetcher.
  //
  // id - the id given to the TranslateURLFetcher on creation
  // success - true of the download was successful
  // data - the body of the downloads response
  void OnDownloadComplete(int id, bool success, const std::string& data);

  // Transfers ownership of |model| to the client using the
  // |internal_on_model_available_cb_|. |is_finished| denotes whether the
  // backend is finished (or has given up on) loading the model.
  void TransferModelToClient(
      std::unique_ptr<chrome_intelligence::RankerModel> model,
      bool is_finished);

  // Validates that ranker model loader backend tasks are all performed on the
  // same sequence.
  base::SequenceChecker sequence_checker_;

  // The TaskRunner on which |this| was constructed.
  const scoped_refptr<base::SequencedTaskRunner> origin_task_runner_;

  // Validates a ranker model on behalf of the model loader client. This may
  // be called on any sequence and must, therefore, be thread-safe.
  const ValidateModelCallback validate_model_cb_;

  // Transfers ownership of a loaded model back to the model loader client.
  // This will be called on the sequence on which the model loader was
  // constructed.
  const InternalOnModelAvailableCallback internal_on_model_available_cb_;

  // The path at which the model is (or should be) cached.
  const base::FilePath model_path_;

  // The URL from which to download the model if the model is not in the cache
  // or the cached model is invalid/expired.
  const GURL model_url_;

  // This will prefix all UMA metrics generated by the model loader.
  const std::string uma_prefix_;

  // Used to download model data from |model_url_|.
  // TODO(rogerm): Use net::URLFetcher directly?
  std::unique_ptr<TranslateURLFetcher> url_fetcher_;

  // The next time before which no new attempts to download the model should be
  // attempted.
  base::TimeTicks next_earliest_download_time_;

  // Tracks the last time of the last attempt to download a model. Used for UMA
  // reporting of download duration.
  base::TimeTicks download_start_time_;

  DISALLOW_COPY_AND_ASSIGN(Backend);
};

RankerModelLoader::Backend::Backend(
    const ValidateModelCallback& validate_model_cb,
    const InternalOnModelAvailableCallback& internal_on_model_available_cb,
    const base::FilePath& model_path,
    const GURL& model_url,
    const std::string& uma_prefix)
    : origin_task_runner_(base::SequencedTaskRunnerHandle::Get()),
      validate_model_cb_(validate_model_cb),
      internal_on_model_available_cb_(internal_on_model_available_cb),
      model_path_(model_path),
      model_url_(model_url),
      uma_prefix_(uma_prefix) {
  sequence_checker_.DetachFromSequence();
}

RankerModelLoader::Backend::~Backend() {}

RankerModelStatus RankerModelLoader::Backend::ReportModelStatus(
    RankerModelStatus model_status) {
  base::HistogramBase* histogram = base::LinearHistogram::FactoryGet(
      uma_prefix_ + kModelStatusHistogram, 1,
      static_cast<int>(RankerModelStatus::MAX),
      static_cast<int>(RankerModelStatus::MAX) + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  if (histogram)
    histogram->Add(static_cast<int>(model_status));
  return model_status;
}

std::unique_ptr<chrome_intelligence::RankerModel>
RankerModelLoader::Backend::CreateModel(const std::string& data) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  MyScopedHistogramTimer timer(uma_prefix_ + kParsetimerHistogram);
  auto model = RankerModel::FromString(data);
  if (ReportModelStatus(model ? validate_model_cb_.Run(*model)
                              : RankerModelStatus::PARSE_FAILED) !=
      RankerModelStatus::OK) {
    return nullptr;
  }
  return model;
}

void RankerModelLoader::Backend::LoadFromFile() {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  // If there is not cache path set, move on to loading the model by URL.
  if (model_path_.empty()) {
    AsyncLoadFromURL();
    return;
  }

  DVLOG(2) << "Attempting to load model from: " << model_path_.value();

  std::string data;

  {
    MyScopedHistogramTimer timer(uma_prefix_ + kReadTimerHistogram);
    if (!base::ReadFileToString(model_path_, &data) || data.empty()) {
      DVLOG(2) << "Failed to read model from: " << model_path_.value();
      data.clear();
    }
  }

  // If model data was loaded, check if it can be parsed to a valid model.
  if (!data.empty()) {
    auto model = CreateModel(data);
    if (model) {
      // The model is valid. The client is willing/able to use it. Keep track
      // of where it originated and whether or not is has expired.
      std::string url_spec = model->GetSourceURL();
      bool is_expired = model->IsExpired();
      bool is_finished = url_spec == model_url_.spec() && !is_expired;

      DVLOG(2) << (is_expired ? "Expired m" : "M") << "odel in '"
               << model_path_.value() << "' was originally downloaded from '"
               << url_spec << "'.";

      // Transfer the model to the client. Beyond this line, |model| is invalid.
      TransferModelToClient(std::move(model), is_finished);

      // If the cached model came from currently configured |model_url_| and has
      // not expired, there is no need schedule a model download.
      if (is_finished)
        return;

      // Otherwise, fall out of this block to schedule a download. The client
      // can continue to use the valid but expired model until the download
      // completes.
    }
  }

  // Reaching this point means that a model download is required. If there is
  // no download URL configured, then there is nothing further to do.
  AsyncLoadFromURL();
}

void RankerModelLoader::Backend::AsyncLoadFromURL() {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  if (!model_url_.is_valid())
    return;

  // Do nothing if the download attempts should be throttled.
  if (base::TimeTicks::Now() < next_earliest_download_time_) {
    DVLOG(2) << "Last download attempt was too recent.";
    return;
  }

  // Otherwise, initialize the model fetcher to be non-null and trigger an
  // initial download attempt.
  if (!url_fetcher_) {
    url_fetcher_ = base::MakeUnique<TranslateURLFetcher>(kUrlFetcherId);
    url_fetcher_->set_max_retry_on_5xx(kMaxRetryOn5xx);
  }

  // If a request is already in flight, do not issue a new one.
  if (url_fetcher_->state() == TranslateURLFetcher::REQUESTING) {
    DVLOG(2) << "Download is in progress.";
    return;
  }

  DVLOG(2) << "Downloading model from: " << model_url_;

  // Reset the time of the next earliest allowable download attempt.
  next_earliest_download_time_ =
      base::TimeTicks::Now() + base::TimeDelta::FromMinutes(kMinRetryDelayMins);

  // Kick off the next download attempt.
  download_start_time_ = base::TimeTicks::Now();
  bool result = url_fetcher_->Request(
      model_url_, base::Bind(&RankerModelLoader::Backend::OnDownloadComplete,
                             base::Unretained(this)));

  // The maximum number of download attempts has been surpassed. Don't make
  // any further attempts.
  if (!result) {
    DVLOG(2) << "Model download abandoned.";
    ReportModelStatus(RankerModelStatus::DOWNLOAD_FAILED);
    url_fetcher_.reset();

    // Notify the loader that model loading has been abandoned.
    TransferModelToClient(nullptr, true);
  }
}

void RankerModelLoader::Backend::OnDownloadComplete(int /* id */,
                                                    bool success,
                                                    const std::string& data) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  // Record the duration of the download.
  base::TimeDelta duration = base::TimeTicks::Now() - download_start_time_;
  base::HistogramBase* counter = base::Histogram::FactoryTimeGet(
      uma_prefix_ + kDownloadTimerHistogram,
      base::TimeDelta::FromMilliseconds(10),
      base::TimeDelta::FromMilliseconds(200000), 100,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  if (counter)
    counter->AddTime(duration);

  // On failure, we just abort. The TranslateRanker will retry on a subsequent
  // translation opportunity. The TranslateURLFetcher enforces a limit for
  // retried requests.
  if (!success || data.empty()) {
    DVLOG(2) << "Download from '" << model_url_ << "'' failed.";
    return;
  }

  auto model = CreateModel(data);
  if (!model) {
    DVLOG(2) << "Model from '" << model_url_ << "'' not valid.";
    return;
  }

  url_fetcher_.reset();

  auto* metadata = model->mutable_proto()->mutable_metadata();
  metadata->set_source(model_url_.spec());
  metadata->set_last_modified_sec(
      (base::Time::Now() - base::Time()).InSeconds());

  if (!model_path_.empty()) {
    DVLOG(2) << "Saving model from '" << model_url_ << "'' to '"
             << model_path_.value() << "'.";
    MyScopedHistogramTimer timer(uma_prefix_ + kWriteTimerHistogram);
    base::ImportantFileWriter::WriteFileAtomically(model_path_,
                                                   model->SerializeAsString());
  }

  // Notify the owner that a compatible model is available.
  TransferModelToClient(std::move(model), true);
}

void RankerModelLoader::Backend::TransferModelToClient(
    std::unique_ptr<chrome_intelligence::RankerModel> model,
    bool is_finished) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  origin_task_runner_->PostTask(
      FROM_HERE, base::Bind(internal_on_model_available_cb_,
                            base::Passed(std::move(model)), is_finished));
}

// =============================================================================
// RankerModelLoader

RankerModelLoader::RankerModelLoader(
    const ValidateModelCallback& validate_model_cb,
    const OnModelAvailableCallback& on_model_available_cb,
    const base::FilePath& model_path,
    const GURL& model_url,
    const std::string& uma_prefix)
    : backend_task_runner_(base::CreateSequencedTaskRunnerWithTraits(
          base::TaskTraits()
              .MayBlock()
              .WithPriority(base::TaskPriority::BACKGROUND)
              .WithShutdownBehavior(
                  base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN))),
      weak_ptr_factory_(this) {
  auto internal_on_model_available_cb =
      base::Bind(&RankerModelLoader::InternalOnModelAvailable,
                 weak_ptr_factory_.GetWeakPtr(), on_model_available_cb);
  backend_ = base::MakeUnique<Backend>(validate_model_cb,
                                       internal_on_model_available_cb,
                                       model_path, model_url, uma_prefix);
}

RankerModelLoader::~RankerModelLoader() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  // This is guaranteed to be sequenced after any pending backend operation.
  backend_task_runner_->DeleteSoon(FROM_HERE, backend_.release());
}

void RankerModelLoader::Start() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DCHECK_EQ(state_, LoaderState::NOT_STARTED);
  state_ = LoaderState::RUNNING;
  backend_task_runner_->PostTask(
      FROM_HERE, base::Bind(&RankerModelLoader::Backend::LoadFromFile,
                            base::Unretained(backend_.get())));
}

void RankerModelLoader::NotifyOfRankerActivity() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  if (state_ == LoaderState::RUNNING) {
    backend_task_runner_->PostTask(
        FROM_HERE, base::Bind(&RankerModelLoader::Backend::AsyncLoadFromURL,
                              base::Unretained(backend_.get())));
  }
}

void RankerModelLoader::InternalOnModelAvailable(
    const OnModelAvailableCallback& callback,
    std::unique_ptr<RankerModel> model,
    bool finished) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  if (finished)
    state_ = LoaderState::FINISHED;
  if (model)
    callback.Run(std::move(model));
}

}  // namespace translate
