// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares helper methods used to schedule files for deletion
// on next reboot.

#ifndef CHROME_INSTALLER_UTIL_DELETE_AFTER_REBOOT_HELPER_H_
#define CHROME_INSTALLER_UTIL_DELETE_AFTER_REBOOT_HELPER_H_

#include <string>
#include <vector>

#include <windows.h>

// Used by the unit tests.
extern const wchar_t kSessionManagerKey[];
extern const wchar_t kPendingFileRenameOps[];

typedef std::pair<std::wstring, std::wstring> PendingMove;

// Attempts to schedule only the item at path for deletion.
bool ScheduleFileSystemEntityForDeletion(const wchar_t* path);

// Attempts to recursively schedule the directory for deletion.
bool ScheduleDirectoryForDeletion(const wchar_t* dir_name);

// Removes all pending moves that are registered for |directory| and all
// elements contained in |directory|.
bool RemoveFromMovesPendingReboot(const wchar_t* directory);

// Retrieves the list of pending renames from the registry and returns a vector
// containing pairs of strings that represent the operations. If the list
// contains only deletes then every other element will be an empty string
// as per http://msdn.microsoft.com/en-us/library/aa365240(VS.85).aspx.
HRESULT GetPendingMovesValue(std::vector<PendingMove>* pending_moves);

// This returns true if |short_form_needle| is contained in |reg_path| where
// |short_form_needle| is a file system path that has been shortened by
// GetShortPathName and |reg_path| is a path stored in the
// PendingFileRenameOperations key.
bool MatchPendingDeletePath(const std::wstring& short_form_needle,
                            const std::wstring& reg_path);

// Converts the strings found in |buffer| to a list of PendingMoves that is
// returned in |value|.
// |buffer| points to a series of pairs of null-terminated wchar_t strings
// followed by a terminating null character.
// |byte_count| is the length of |buffer| in bytes.
// |value| is a pointer to an empty vector of PendingMoves (string pairs).
// On success, this vector contains all of the string pairs extracted from
// |buffer|.
// Returns S_OK on success, E_INVALIDARG if buffer does not meet the above
// specification.
HRESULT MultiSZBytesToStringArray(const char* buffer, size_t byte_count,
                                  std::vector<PendingMove>* value);

// The inverse of MultiSZBytesToStringArray, this function converts a list
// of string pairs into a byte array format suitable for writing to the
// kPendingFileRenameOps registry value. It concatenates the strings and
// appends an additional terminating null character.
void StringArrayToMultiSZBytes(const std::vector<PendingMove>& strings,
                               std::vector<char>* buffer);

// A helper function for the win32 GetShortPathName that more conveniently
// returns a correctly sized wstring.
std::wstring GetShortPathName(const wchar_t* path);

#endif  // CHROME_INSTALLER_UTIL_DELETE_AFTER_REBOOT_HELPER_H_
