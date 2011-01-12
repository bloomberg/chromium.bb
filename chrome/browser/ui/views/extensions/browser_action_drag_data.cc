// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/browser_action_drag_data.h"

#include "base/logging.h"
#include "base/pickle.h"
#include "base/string_util.h"
#include "chrome/browser/profiles/profile.h"

const char* BrowserActionDragData::kClipboardFormatString =
    "chromium/x-browser-actions";

BrowserActionDragData::BrowserActionDragData()
    : profile_id_(0), index_(-1) {
}

BrowserActionDragData::BrowserActionDragData(
    const std::string& id, int index)
    : profile_id_(0), id_(id), index_(index) {
}

bool BrowserActionDragData::IsFromProfile(Profile* profile) const {
  return (profile_id_ == profile->GetRuntimeId());
}

#if defined(TOOLKIT_VIEWS)
void BrowserActionDragData::Write(
    Profile* profile, ui::OSExchangeData* data) const {
  DCHECK(data);
  Pickle data_pickle;
  WriteToPickle(profile, &data_pickle);
  data->SetPickledData(GetBrowserActionCustomFormat(), data_pickle);
}

bool BrowserActionDragData::Read(const ui::OSExchangeData& data) {
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
ui::OSExchangeData::CustomFormat
    BrowserActionDragData::GetBrowserActionCustomFormat() {
  static ui::OSExchangeData::CustomFormat format;
  static bool format_valid = false;

  if (!format_valid) {
    format_valid = true;
    format = ui::OSExchangeData::RegisterCustomFormat(
        BrowserActionDragData::kClipboardFormatString);
  }
  return format;
}
#endif

void BrowserActionDragData::WriteToPickle(
    Profile* profile, Pickle* pickle) const {
  ProfileId profile_id = profile->GetRuntimeId();
  pickle->WriteBytes(&profile_id, sizeof(profile_id));
  pickle->WriteString(id_);
  pickle->WriteSize(index_);
}

bool BrowserActionDragData::ReadFromPickle(Pickle* pickle) {
  void* data_iterator = NULL;

  const char* tmp;
  if (!pickle->ReadBytes(&data_iterator, &tmp, sizeof(profile_id_)))
    return false;
  memcpy(&profile_id_, tmp, sizeof(profile_id_));

  if (!pickle->ReadString(&data_iterator, &id_))
    return false;

  if (!pickle->ReadSize(&data_iterator, &index_))
    return false;

  return true;
}
