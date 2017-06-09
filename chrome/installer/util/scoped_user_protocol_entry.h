// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_SCOPED_USER_PROTOCOL_ENTRY_H_
#define CHROME_INSTALLER_UTIL_SCOPED_USER_PROTOCOL_ENTRY_H_

#include <memory>
#include <vector>

#include "base/macros.h"

class RegistryEntry;

// Windows 8 shows the "No apps are installed to open this type of link"
// dialog when choosing a default handler for a |protocol| under certain
// circumstances. Under these circumstances, it appears that ensuring the
// existence of the HKCU\Software\Classes\<protocol> key with an empty "URL
// Protocol" value is sufficient to make the dialog contain the usual list of
// registered browsers. This class creates this key and value in its constructor
// if needed, and cleans them up in its destructor if no other values or subkeys
// were created in the meantime. For details, see https://crbug.com/569151.
class ScopedUserProtocolEntry {
 public:
  explicit ScopedUserProtocolEntry(const wchar_t* protocol);
  ~ScopedUserProtocolEntry();

 private:
  std::vector<std::unique_ptr<RegistryEntry>> entries_;

  DISALLOW_COPY_AND_ASSIGN(ScopedUserProtocolEntry);
};

#endif  // CHROME_INSTALLER_UTIL_SCOPED_USER_PROTOCOL_ENTRY_H_
