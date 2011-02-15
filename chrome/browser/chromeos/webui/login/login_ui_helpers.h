// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WEBUI_LOGIN_LOGIN_UI_HELPERS_H_
#define CHROME_BROWSER_CHROMEOS_WEBUI_LOGIN_LOGIN_UI_HELPERS_H_
#pragma once

#include <string>

class Browser;
class DictionaryValue;
class FilePath;
class Profile;
class RefCountedBytes;

namespace base {
class StringPiece;
};  // namespace base

namespace chromeos {

// This class is used for encapsulating the statics and other other messy
// external calls that are required for getting the needed profile objects. This
// allows for easier mocking of this code and allows for modularity.
class ProfileOperationsInterface {
 public:
  ProfileOperationsInterface() {}
  virtual ~ProfileOperationsInterface() {}

  virtual Profile* GetDefaultProfile();
  virtual Profile* GetDefaultProfileByPath();

 private:
  Profile* GetDefaultProfile(FilePath user_data_dir);
  FilePath GetUserDataPath();

  DISALLOW_COPY_AND_ASSIGN(ProfileOperationsInterface);
};

// This class is used for encapsulating the statics and other other messy
// external calls that are required for creating and getting the needed browser
// objects. This allows for easier mocking of this code and allows for
// modularity.
class BrowserOperationsInterface {
 public:
  BrowserOperationsInterface() {}
  virtual ~BrowserOperationsInterface() {}

  virtual Browser* CreateBrowser(Profile* profile);
  virtual Browser* GetLoginBrowser(Profile* profile);

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserOperationsInterface);
};

// This class is used for encapsulating the statics and other other messy
// external calls that are required for creating and getting the needed HTML
// objects. This allows for easier mocking of this code and allows for
// modularity. Since we don't currently unit the class that this code is related
// to, there is a case for refactoring it back into LoginUIHTMLSource.
class HTMLOperationsInterface {
 public:
  HTMLOperationsInterface() {}
  virtual ~HTMLOperationsInterface() {}

  virtual base::StringPiece GetLoginHTML();
  virtual std::string GetFullHTML(base::StringPiece login_html,
                                  DictionaryValue* localized_strings);
  virtual RefCountedBytes* CreateHTMLBytes(std::string full_html);

 private:
  DISALLOW_COPY_AND_ASSIGN(HTMLOperationsInterface);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_WEBUI_LOGIN_LOGIN_UI_HELPERS_H_
