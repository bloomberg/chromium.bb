// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/image_writer_utility_client.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/common/extensions/removable_storage_writer.mojom.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/utility_process_mojo_client.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {
namespace image_writer {

namespace {
ImageWriterUtilityClient::ImageWriterUtilityClientFactory*
    g_factory_for_testing = nullptr;
}  // namespace

class ImageWriterUtilityClient::RemovableStorageWriterClientImpl
    : public extensions::mojom::RemovableStorageWriterClient {
 public:
  RemovableStorageWriterClientImpl(
      ImageWriterUtilityClient* owner,
      extensions::mojom::RemovableStorageWriterClientPtr* interface)
      : binding_(this, mojo::MakeRequest(interface)),
        image_writer_utility_client_(owner) {
    base::ThreadRestrictions::AssertIOAllowed();

    binding_.set_connection_error_handler(
        base::BindOnce(&ImageWriterUtilityClient::UtilityProcessError,
                       image_writer_utility_client_));
  }

  ~RemovableStorageWriterClientImpl() override = default;

 private:
  void Progress(int64_t progress) override {
    image_writer_utility_client_->OperationProgress(progress);
  }

  void Complete(const base::Optional<std::string>& error) override {
    if (error) {
      image_writer_utility_client_->OperationFailed(error.value());
    } else {
      image_writer_utility_client_->OperationSucceeded();
    }
  }

  mojo::Binding<extensions::mojom::RemovableStorageWriterClient> binding_;
  // |image_writer_utility_client_| owns |this|.
  ImageWriterUtilityClient* const image_writer_utility_client_;

  DISALLOW_COPY_AND_ASSIGN(RemovableStorageWriterClientImpl);
};

ImageWriterUtilityClient::ImageWriterUtilityClient() = default;

ImageWriterUtilityClient::~ImageWriterUtilityClient() = default;

// static
scoped_refptr<ImageWriterUtilityClient> ImageWriterUtilityClient::Create() {
  if (g_factory_for_testing)
    return g_factory_for_testing->Run();
  return base::WrapRefCounted(new ImageWriterUtilityClient());
}

// static
void ImageWriterUtilityClient::SetFactoryForTesting(
    ImageWriterUtilityClientFactory* factory) {
  g_factory_for_testing = factory;
}

void ImageWriterUtilityClient::Write(const ProgressCallback& progress_callback,
                                     const SuccessCallback& success_callback,
                                     const ErrorCallback& error_callback,
                                     const base::FilePath& source,
                                     const base::FilePath& target) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!removable_storage_writer_client_);

  progress_callback_ = progress_callback;
  success_callback_ = success_callback;
  error_callback_ = error_callback;

  StartUtilityProcessIfNeeded();

  extensions::mojom::RemovableStorageWriterClientPtr client;
  removable_storage_writer_client_ =
      base::MakeUnique<RemovableStorageWriterClientImpl>(this, &client);

  utility_process_mojo_client_->service()->Write(source, target,
                                                 std::move(client));
}

void ImageWriterUtilityClient::Verify(const ProgressCallback& progress_callback,
                                      const SuccessCallback& success_callback,
                                      const ErrorCallback& error_callback,
                                      const base::FilePath& source,
                                      const base::FilePath& target) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!removable_storage_writer_client_);

  progress_callback_ = progress_callback;
  success_callback_ = success_callback;
  error_callback_ = error_callback;

  StartUtilityProcessIfNeeded();

  extensions::mojom::RemovableStorageWriterClientPtr client;
  removable_storage_writer_client_ =
      base::MakeUnique<RemovableStorageWriterClientImpl>(this, &client);

  utility_process_mojo_client_->service()->Verify(source, target,
                                                  std::move(client));
}

void ImageWriterUtilityClient::Cancel(const CancelCallback& cancel_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(cancel_callback);

  ResetRequest();
  base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE, cancel_callback);
}

void ImageWriterUtilityClient::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ResetRequest();
  utility_process_mojo_client_.reset();
}

void ImageWriterUtilityClient::StartUtilityProcessIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (utility_process_mojo_client_)
    return;

  utility_process_mojo_client_ =
      base::MakeUnique<content::UtilityProcessMojoClient<
          extensions::mojom::RemovableStorageWriter>>(
          l10n_util::GetStringUTF16(IDS_UTILITY_PROCESS_IMAGE_WRITER_NAME));
  utility_process_mojo_client_->set_error_callback(
      base::Bind(&ImageWriterUtilityClient::UtilityProcessError, this));

  utility_process_mojo_client_->set_disable_sandbox();
#if defined(OS_WIN)
  utility_process_mojo_client_->set_run_elevated();
#endif

  utility_process_mojo_client_->Start();
}

void ImageWriterUtilityClient::UtilityProcessError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  OperationFailed("Utility process crashed or failed.");
  utility_process_mojo_client_.reset();
}

void ImageWriterUtilityClient::OperationProgress(int64_t progress) {
  if (progress_callback_)
    progress_callback_.Run(progress);
}

void ImageWriterUtilityClient::OperationSucceeded() {
  SuccessCallback success_callback = success_callback_;
  ResetRequest();
  if (success_callback)
    success_callback.Run();
}

void ImageWriterUtilityClient::OperationFailed(const std::string& error) {
  ErrorCallback error_callback = error_callback_;
  ResetRequest();
  if (error_callback)
    error_callback.Run(error);
}

void ImageWriterUtilityClient::ResetRequest() {
  removable_storage_writer_client_.reset();

  // Clear handlers to not hold any reference to the caller.
  progress_callback_.Reset();
  success_callback_.Reset();
  error_callback_.Reset();
}

}  // namespace image_writer
}  // namespace extensions
