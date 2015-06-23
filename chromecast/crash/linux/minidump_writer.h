// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CRASH_LINUX_MINIDUMP_WRITER_H_
#define CHROMECAST_CRASH_LINUX_MINIDUMP_WRITER_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "chromecast/crash/linux/minidump_params.h"
#include "chromecast/crash/linux/synchronized_minidump_manager.h"

namespace chromecast {

class MinidumpGenerator;

// Class for writing a minidump with synchronized access to the minidumps
// directory.
class MinidumpWriter : public SynchronizedMinidumpManager {
 public:
  typedef base::Callback<int(const std::string&)> DumpStateCallback;

  // Constructs a writer for a minidump. If |minidump_filename| is absolute, it
  // must be a path to a file in the |dump_path_| directory. Otherwise, it
  // should be a filename only, in which case, |minidump_generator| creates
  // a minidump at $HOME/minidumps/|minidump_filename|. |params| describes the
  // minidump metadata. |dump_state_cb| is Run() to generate a log dump. Please
  // see the comments on |dump_state_cb_| below for details about this
  // parameter.
  // This does not take ownership of |minidump_generator|.
  MinidumpWriter(MinidumpGenerator* minidump_generator,
                 const std::string& minidump_filename,
                 const MinidumpParams& params,
                 const base::Callback<int(const std::string&)>& dump_state_cb);

  // Like the constructor above, but the default implementation of
  // |dump_state_cb_| is used inside DoWork().
  MinidumpWriter(MinidumpGenerator* minidump_generator,
                 const std::string& minidump_filename,
                 const MinidumpParams& params);

  ~MinidumpWriter() override;

  // Acquires exclusive access to the minidumps directory and generates a
  // minidump and a log to be uploaded later. Returns 0 if successful, -1
  // otherwise.
  int Write() { return AcquireLockAndDoWork(); }

  int max_dumps() const { return max_dumps_; }
  int max_recent_dumps() const { return max_recent_dumps_; }
  const base::TimeDelta& dump_interval() const { return dump_interval_; };

 protected:
  // MinidumpManager implementation:
  int DoWork() override;

 private:
  // Returns true if we can write another dump, false otherwise. We can write
  // another dump if the number of minidumps is strictly less than |max_dumps_|
  // and the number of minidumps which occurred within the last |dump_interval_|
  // is strictly less than |max_recent_dumps_|.
  bool CanWriteDump();

  MinidumpGenerator* const minidump_generator_;
  base::FilePath minidump_path_;
  const MinidumpParams params_;

  const int max_dumps_;
  const base::TimeDelta dump_interval_;
  const int max_recent_dumps_;

  // This callback is Run() to dump a log to |minidump_path_|.txt.gz, taking
  // |minidump_path_| as a parameter. It returns 0 on success, and a negative
  // integer otherwise. If a callback is not passed in the constructor, the
  // default implemementaion is used.
  DumpStateCallback dump_state_cb_;

  DISALLOW_COPY_AND_ASSIGN(MinidumpWriter);
};

}  // namespace chromecast

#endif  // CHROMECAST_CRASH_LINUX_MINIDUMP_WRITER_H_
