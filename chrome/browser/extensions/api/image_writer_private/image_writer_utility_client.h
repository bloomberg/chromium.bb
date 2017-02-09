// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_IMAGE_WRITER_UTILITY_CLIENT_H_
#define CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_IMAGE_WRITER_UTILITY_CLIENT_H_

#include <stdint.h>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "chrome/common/extensions/removable_storage_writer.mojom.h"
#include "content/public/browser/utility_process_mojo_client.h"

// Writes a disk image to a device inside the utility process.
class ImageWriterUtilityClient
    : public base::RefCountedThreadSafe<ImageWriterUtilityClient> {
 public:
  typedef base::Callback<void()> CancelCallback;
  typedef base::Callback<void()> SuccessCallback;
  typedef base::Callback<void(int64_t)> ProgressCallback;
  typedef base::Callback<void(const std::string&)> ErrorCallback;

  ImageWriterUtilityClient();

  // Starts the write operation.
  // |progress_callback|: Called periodically with the count of bytes processed.
  // |success_callback|: Called at successful completion.
  // |error_callback|: Called with an error message on failure.
  // |source|: The path to the source file to read data from.
  // |target|: The path to the target device to write the image file to.
  virtual void Write(const ProgressCallback& progress_callback,
                     const SuccessCallback& success_callback,
                     const ErrorCallback& error_callback,
                     const base::FilePath& source,
                     const base::FilePath& target);

  // Starts a verify operation.
  // |progress_callback|: Called periodically with the count of bytes processed.
  // |success_callback|: Called at successful completion.
  // |error_callback|: Called with an error message on failure.
  // |source|: The path to the source file to read data from.
  // |target|: The path to the target device file to verify.
  virtual void Verify(const ProgressCallback& progress_callback,
                      const SuccessCallback& success_callback,
                      const ErrorCallback& error_callback,
                      const base::FilePath& source,
                      const base::FilePath& target);

  // Cancels any pending write or verify operation.
  // |cancel_callback|: Called when the cancel has actually occurred.
  virtual void Cancel(const CancelCallback& cancel_callback);

  // Shuts down the utility process that may have been created.
  virtual void Shutdown();

 protected:
  friend class base::RefCountedThreadSafe<ImageWriterUtilityClient>;

  virtual ~ImageWriterUtilityClient();

 private:
  class RemovableStorageWriterClientImpl;

  // Ensures the utility process has been created.
  void StartUtilityProcess();

  void UtilityProcessError();

  void OperationProgress(int64_t progress);
  void OperationSucceeded();
  void OperationFailed(const std::string& error);

  void ResetRequest();

  ProgressCallback progress_callback_;
  SuccessCallback success_callback_;
  ErrorCallback error_callback_;

  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  std::unique_ptr<content::UtilityProcessMojoClient<
      extensions::mojom::RemovableStorageWriter>>
      utility_process_mojo_client_;

  std::unique_ptr<RemovableStorageWriterClientImpl>
      removable_storage_writer_client_;

  DISALLOW_COPY_AND_ASSIGN(ImageWriterUtilityClient);
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_IMAGE_WRITER_UTILITY_CLIENT_H_
