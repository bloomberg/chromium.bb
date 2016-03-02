// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_DELETE_OLD_VERSIONS_H_
#define CHROME_INSTALLER_UTIL_DELETE_OLD_VERSIONS_H_

class WorkItemList;

namespace installer {

class InstallerState;

// Adds to |work_item_list| a WorkItem which deletes files that belong to old
// versions of Chrome. chrome.exe, new_chrome.exe and their associated version
// directories are never deleted. Also, no file is deleted for a given version
// if a .exe or .dll file for that version is in use. |installer_state| must be
// alive when |work_item_list| is executed.
void AddDeleteOldVersionsWorkItem(const InstallerState& installer_state,
                                  WorkItemList* work_item_list);

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_DELETE_OLD_VERSIONS_H_
