// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_OPERATION_H_
#define CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_OPERATION_H_

#include "base/callback.h"
#include "base/md5.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/extensions/api/image_writer_private/image_writer_utils.h"
#include "chrome/common/cancelable_task_tracker.h"
#include "chrome/common/extensions/api/image_writer_private.h"

namespace image_writer_api = extensions::api::image_writer_private;

namespace base {
class FilePath;
}  // namespace base

namespace extensions {
namespace image_writer {

const int kProgressComplete = 100;

class OperationManager;

// Encapsulates an operation being run on behalf of the
// OperationManager.  Construction of the operation does not start
// anything.  The operation's Start method should be called to start it, and
// then the Cancel method will stop it.  The operation will call back to the
// OperationManager periodically or on any significant event.
//
// Each stage of the operation is generally divided into three phases: Start,
// Run, Complete.  Start and Complete run on the UI thread and are responsible
// for advancing to the next stage and other UI interaction.  The Run phase does
// the work on the FILE thread and calls SendProgress or Error as appropriate.
class Operation
    : public base::RefCountedThreadSafe<Operation> {
 public:
  typedef base::Callback<void(bool, const std::string&)> StartWriteCallback;
  typedef base::Callback<void(bool, const std::string&)> CancelWriteCallback;
  typedef std::string ExtensionId;

  Operation(base::WeakPtr<OperationManager> manager,
            const ExtensionId& extension_id,
            const std::string& storage_unit_id);

  // Starts the operation.
  virtual void Start() = 0;

  // Cancel the operation. This must be called to clean up internal state and
  // cause the the operation to actually stop.  It will not be destroyed until
  // all callbacks have completed.
  void Cancel();

  // Aborts the operation, cancelling it and generating an error.
  void Abort();
 protected:
  virtual ~Operation();

  // Generates an error.
  // |error_message| is used to create an OnWriteError event which is
  // sent to the extension
  virtual void Error(const std::string& error_message);

  // Set |progress_| and send an event.  Progress should be in the interval
  // [0,100]
  void SetProgress(int progress);
  // Change to a new |stage_| and set |progress_| to zero.
  void SetStage(image_writer_api::Stage stage);

  // Can be queried to safely determine if the operation has been cancelled.
  bool IsCancelled();

  // Adds a callback that will be called during clean-up, whether the operation
  // is aborted, encounters and error, or finishes successfully.  These
  // functions will be run on the FILE thread.
  void AddCleanUpFunction(base::Closure);

  void UnzipStart(scoped_ptr<base::FilePath> zip_file);
  void WriteStart();
  void VerifyWriteStart();
  void Finish();

  // If |file_size| is non-zero, only |file_size| bytes will be read from file,
  // otherwise the entire file will be read.
  // |progress_scale| is a percentage to which the progress will be scale, e.g.
  // a scale of 50 means it will increment from 0 to 50 over the course of the
  // sum.  |progress_offset| is an percentage that will be added to the progress
  // of the MD5 sum before updating |progress_| but after scaling.
  void GetMD5SumOfFile(
      scoped_ptr<base::FilePath> file,
      int64 file_size,
      int progress_offset,
      int progress_scale,
      const base::Callback<void(scoped_ptr<std::string>)>& callback);

  base::WeakPtr<OperationManager> manager_;
  const ExtensionId extension_id_;

  base::FilePath image_path_;
  const std::string storage_unit_id_;
 private:
  friend class base::RefCountedThreadSafe<Operation>;

  // TODO(haven): Clean up these switches. http://crbug.com/292956
#if defined(OS_LINUX) && !defined(CHROMEOS)
  void WriteRun();
  void WriteChunk(scoped_ptr<image_writer_utils::ImageReader> reader,
                  scoped_ptr<image_writer_utils::ImageWriter> writer,
                  int64 bytes_written);
  bool WriteCleanUp(scoped_ptr<image_writer_utils::ImageReader> reader,
                    scoped_ptr<image_writer_utils::ImageWriter> writer);
  void WriteComplete();

  void VerifyWriteStage2(scoped_ptr<std::string> image_hash);
  void VerifyWriteCompare(scoped_ptr<std::string> image_hash,
                          scoped_ptr<std::string> device_hash);
#endif

#if defined(OS_CHROMEOS)
  void StartWriteOnUIThread();

  void OnBurnFinished(const std::string& target_path,
                      bool success,
                      const std::string& error);
  void OnBurnProgress(const std::string& target_path,
                      int64 num_bytes_burnt,
                      int64 total_size);
  void OnBurnError();
#endif

  void MD5Chunk(scoped_ptr<image_writer_utils::ImageReader> reader,
                int64 bytes_processed,
                int64 bytes_total,
                int progress_offset,
                int progress_scale,
                const base::Callback<void(scoped_ptr<std::string>)>& callback);

  // Runs all cleanup functions.
  void CleanUp();

  // |stage_| and |progress_| are owned by the FILE thread, use |SetStage| and
  // |SetProgress| to update.  Progress should be in the interval [0,100]
  image_writer_api::Stage stage_;
  int progress_;

  // MD5 contexts don't play well with smart pointers.  Just going to allocate
  // memory here.  This requires that we only do one MD5 sum at a time.
  base::MD5Context md5_context_;

  // CleanUp operations that must be run.  All these functions are run on the
  // FILE thread.
  std::vector<base::Closure> cleanup_functions_;
};

}  // namespace image_writer
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_OPERATION_H_
