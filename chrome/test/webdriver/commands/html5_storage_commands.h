// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_COMMANDS_HTML5_STORAGE_COMMANDS_H_
#define CHROME_TEST_WEBDRIVER_COMMANDS_HTML5_STORAGE_COMMANDS_H_

#include <string>
#include <vector>

#include "chrome/test/webdriver/commands/webdriver_command.h"

namespace base {
class DictionaryValue;
}

namespace webdriver {

class Response;

class LocalStorageCommand : public WebDriverCommand {
 public:
  LocalStorageCommand(const std::vector<std::string>& path_segments,
                      const base::DictionaryValue* const parameters);
  virtual ~LocalStorageCommand();

  virtual bool DoesGet() OVERRIDE;
  virtual bool DoesPost() OVERRIDE;
  virtual bool DoesDelete() OVERRIDE;

  // Returns the list of all keys in the HTML5 localStorage object.
  virtual void ExecuteGet(Response* const response) OVERRIDE;

  // Sets the value of an item in the HTML5 localStorage.
  virtual void ExecutePost(Response* const response) OVERRIDE;

  // Deletes all items from the localStorage object.
  virtual void ExecuteDelete(Response* const response) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(LocalStorageCommand);
};

class LocalStorageKeyCommand : public WebDriverCommand {
 public:
  LocalStorageKeyCommand(const std::vector<std::string>& path_segments,
                         const base::DictionaryValue* const parameters);
  virtual ~LocalStorageKeyCommand();

  virtual bool DoesGet() OVERRIDE;
  virtual bool DoesDelete() OVERRIDE;

  // Returns the value of an item in the HTML5 localStorage.
  virtual void ExecuteGet(Response* const response) OVERRIDE;

  // Deletes an item in the HTML5 localStorage and returns the value.
  virtual void ExecuteDelete(Response* const response) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(LocalStorageKeyCommand);
};

class LocalStorageSizeCommand : public WebDriverCommand {
 public:
  LocalStorageSizeCommand(const std::vector<std::string>& path_segments,
                          const base::DictionaryValue* const parameters);
  virtual ~LocalStorageSizeCommand();

  virtual bool DoesGet() OVERRIDE;

  // Returns the size of the HTML5 localStorage object.
  virtual void ExecuteGet(Response* const response) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(LocalStorageSizeCommand);
};

class SessionStorageCommand : public WebDriverCommand {
 public:
  SessionStorageCommand(const std::vector<std::string>& path_segments,
                        base::DictionaryValue* parameters);
  virtual ~SessionStorageCommand();

  virtual bool DoesGet() OVERRIDE;
  virtual bool DoesPost() OVERRIDE;
  virtual bool DoesDelete() OVERRIDE;

  // Returns the key of all items in the HTML5 sessionStorage object.
  virtual void ExecuteGet(Response* const response) OVERRIDE;

  // Set the value of an item in the HTML5 sessionStorage.
  virtual void ExecutePost(Response* const response) OVERRIDE;

  // Deletes all items from the sessionStorage object.
  virtual void ExecuteDelete(Response* const response) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionStorageCommand);
};

class SessionStorageKeyCommand : public WebDriverCommand {
 public:
  SessionStorageKeyCommand(const std::vector<std::string>& path_segments,
                           const base::DictionaryValue* const parameters);
  virtual ~SessionStorageKeyCommand();

  virtual bool DoesGet() OVERRIDE;
  virtual bool DoesDelete() OVERRIDE;

  // Returns the value of an item in the HTML5 sessionStorage.
  virtual void ExecuteGet(Response* const response) OVERRIDE;

  // Deletes an item in the HTML5 sessionStorage and returns the value.
  virtual void ExecuteDelete(Response* const response) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionStorageKeyCommand);
};

class SessionStorageSizeCommand : public WebDriverCommand {
 public:
  SessionStorageSizeCommand(const std::vector<std::string>& path_segments,
                            const base::DictionaryValue* const parameters);
  virtual ~SessionStorageSizeCommand();

  virtual bool DoesGet() OVERRIDE;

  // Returns the size of the HTML5 sessionStorage object.
  virtual void ExecuteGet(Response* const response) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionStorageSizeCommand);
};

}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_COMMANDS_HTML5_STORAGE_COMMANDS_H_
