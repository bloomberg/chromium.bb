// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_RECOVERY_PRIVATE_RECOVERY_OPERATION_H_
#define CHROME_BROWSER_EXTENSIONS_API_RECOVERY_PRIVATE_RECOVERY_OPERATION_H_

#include "base/callback.h"
#include "base/md5.h"
#include "base/memory/ref_counted_memory.h"
#include "base/timer/timer.h"
#include "chrome/browser/extensions/api/recovery_private/recovery_utils.h"
#include "chrome/common/cancelable_task_tracker.h"
#include "chrome/common/extensions/api/recovery_private.h"

namespace recovery_api = extensions::api::recovery_private;

namespace base {

class FilePath;

} // namespace base

namespace extensions {
namespace recovery {

class RecoveryOperationManager;

// Encapsulates an operation being run on behalf of the
// RecoveryOperationManager.  Construction of the operation does not start
// anything.  The operation's Start method should be called to start it, and
// then the Cancel method will stop it.  The operation will call back to the
// RecoveryOperationManager periodically or on any significant event.
//
// Each stage of the operation is generally divided into three phases: Start,
// Run, Complete.  Start and Complete run on the UI thread and are responsible
// for advancing to the next stage and other UI interaction.  The Run phase does
// the work on the FILE thread and calls SendProgress or Error as appropriate.
class RecoveryOperation : public base::RefCountedThreadSafe<RecoveryOperation> {
 public:
  typedef base::Callback<void(bool, const std::string&)> StartWriteCallback;
  typedef base::Callback<void(bool, const std::string&)> CancelWriteCallback;
  typedef std::string ExtensionId;

  RecoveryOperation(RecoveryOperationManager* manager,
                    const ExtensionId& extension_id,
                    const std::string& storage_unit_id);

  // Starts the operation.
  virtual void Start() = 0;

  // Cancel the operation. This must be called to clean up internal state and
  // cause the the operation to actually stop.  It will not be destroyed until
  // all callbacks have completed.  This method may be overridden to provide
  // subclass cancelling.  It should still call the superclass method.
  virtual void Cancel();

  // Aborts the operation, cancelling it and generating an error.
  virtual void Abort();
 protected:
  virtual ~RecoveryOperation();

  // Generates an error.
  // |error_message| is used to create an OnWriteError event which is
  // sent to the extension
  virtual void Error(const std::string& error_message);
  // Sends a progress notification.
  void SendProgress();

  // Can be queried to safely determine if the operation has been cancelled.
  bool IsCancelled();

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

  RecoveryOperationManager* manager_;
  const ExtensionId extension_id_;

  // This field is owned by the UI thread.
  recovery_api::Stage stage_;

  // The amount of work completed for the current stage as a percentage.  This
  // variable may be modified by both the FILE and UI threads, but it is only
  // modified by the basic activity flow of the operation which is logically
  // single threaded.  Progress events will read this variable from the UI
  // thread which has the potential to read a value newer than when the event
  // was posted.  However, the stage is never advanced except on the UI thread
  // and so will be later in the queue than those progress events.  Thus
  // progress events may report higher progress numbers than when queued, but
  // will never report the wrong stage and appear to move backwards.
  int progress_;

  base::FilePath image_path_;
  const std::string storage_unit_id_;
 private:
  friend class base::RefCountedThreadSafe<RecoveryOperation>;

#if defined(OS_LINUX) && !defined(CHROMEOS)
  void WriteRun();
  void WriteChunk(
      scoped_ptr<recovery_utils::ImageReader> reader,
      scoped_ptr<recovery_utils::ImageWriter> writer,
      int64 bytes_written);
  bool WriteCleanup(
      scoped_ptr<recovery_utils::ImageReader> reader,
      scoped_ptr<recovery_utils::ImageWriter> writer);
  void WriteComplete();

  void VerifyWriteStage2(scoped_ptr<std::string> image_hash);
  void VerifyWriteCompare(scoped_ptr<std::string> image_hash,
                          scoped_ptr<std::string> device_hash);
#endif

  void MD5Chunk(scoped_ptr<recovery_utils::ImageReader> reader,
                int64 bytes_processed,
                int64 bytes_total,
                int progress_offset,
                int progress_scale,
                const base::Callback<void(scoped_ptr<std::string>)>& callback);

  // MD5 contexts don't play well with smart pointers.  Just going to allocate
  // memory here.  This requires that we only do one MD5 sum at a time.
  base::MD5Context md5_context_;

};

} // namespace recovery
} // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_RECOVERY_PRIVATE_RECOVERY_OPERATION_H_
