// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_SAVE_FILE_H_
#define CHROME_BROWSER_DOWNLOAD_SAVE_FILE_H_
#pragma once

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/download/base_file.h"
#include "chrome/browser/download/save_types.h"

// SaveFile ----------------------------------------------------------------

// These objects live exclusively on the file thread and handle the writing
// operations for one save item. These objects live only for the duration that
// the saving job is 'in progress': once the saving job has been completed or
// canceled, the SaveFile is destroyed. One SaveFile object represents one item
// in a save session.
class SaveFile : public BaseFile {
 public:
  explicit SaveFile(const SaveFileCreateInfo* info);
  ~SaveFile();

  // Accessors.
  int save_id() const { return info_->save_id; }
  int render_process_id() const { return info_->render_process_id; }
  int render_view_id() const { return info_->render_view_id; }
  int request_id() const { return info_->request_id; }
  SaveFileCreateInfo::SaveFileSource save_source() const {
    return info_->save_source;
  }

 private:
  scoped_ptr<const SaveFileCreateInfo> info_;

  DISALLOW_COPY_AND_ASSIGN(SaveFile);
};

#endif  // CHROME_BROWSER_DOWNLOAD_SAVE_FILE_H_
