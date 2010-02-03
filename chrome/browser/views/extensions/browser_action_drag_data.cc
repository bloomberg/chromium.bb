// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/extensions/browser_action_drag_data.h"

#include "base/file_path.h"
#include "base/logging.h"
#include "base/pickle.h"
#include "base/string_util.h"
#include "chrome/browser/profile.h"

const char* BrowserActionDragData::kClipboardFormatString =
    "chromium/x-browser-actions";

BrowserActionDragData::BrowserActionDragData()
    : index_(-1) {
}

BrowserActionDragData::BrowserActionDragData(
    const std::string& id, int index)
    : id_(id),
      index_(index) {
}

bool BrowserActionDragData::IsFromProfile(Profile* profile) const {
  // An empty path means the data is not associated with any profile.
  return (!profile_path_.empty() &&
          profile_path_ == profile->GetPath().value());
}

#if defined(TOOLKIT_VIEWS)
void BrowserActionDragData::Write(
    Profile* profile, OSExchangeData* data) const {
  DCHECK(data);
  Pickle data_pickle;
  WriteToPickle(profile, &data_pickle);
  data->SetPickledData(GetBrowserActionCustomFormat(), data_pickle);
}

bool BrowserActionDragData::Read(const OSExchangeData& data) {
  if (!data.HasCustomFormat(GetBrowserActionCustomFormat()))
    return false;

  Pickle drag_data_pickle;
  if (!data.GetPickledData(GetBrowserActionCustomFormat(), &drag_data_pickle))
    return false;

  if (!ReadFromPickle(&drag_data_pickle))
    return false;

  return true;
}

// static
OSExchangeData::CustomFormat
    BrowserActionDragData::GetBrowserActionCustomFormat() {
  static OSExchangeData::CustomFormat format;
  static bool format_valid = false;

  if (!format_valid) {
    format_valid = true;
    format = OSExchangeData::RegisterCustomFormat(
        BrowserActionDragData::kClipboardFormatString);
  }
  return format;
}
#endif

void BrowserActionDragData::WriteToPickle(
    Profile* profile, Pickle* pickle) const {
  FilePath::WriteStringTypeToPickle(pickle, profile->GetPath().value());
  pickle->WriteString(id_);
  pickle->WriteInt(index_);
}

bool BrowserActionDragData::ReadFromPickle(Pickle* pickle) {
  void* data_iterator = NULL;
  if (!FilePath::ReadStringTypeFromPickle(pickle, &data_iterator,
                                          &profile_path_)) {
    return false;
  }

  if (!pickle->ReadString(&data_iterator, &id_))
    return false;

  int index;
  if (!pickle->ReadInt(&data_iterator, &index))
    return false;

  index_ = index;
  return true;
}
