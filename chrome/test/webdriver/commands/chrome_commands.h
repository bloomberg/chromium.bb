// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_COMMANDS_CHROME_COMMANDS_H_
#define CHROME_TEST_WEBDRIVER_COMMANDS_CHROME_COMMANDS_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "chrome/test/webdriver/commands/webdriver_command.h"

namespace base {
class DictionaryValue;
}

namespace webdriver {

class Response;

// Commands dealing with the extensions system.
class ExtensionsCommand : public WebDriverCommand {
 public:
  ExtensionsCommand(const std::vector<std::string>& path_segments,
                    const base::DictionaryValue* const parameters);
  virtual ~ExtensionsCommand();

  virtual bool DoesGet() OVERRIDE;
  virtual bool DoesPost() OVERRIDE;

  // Returns all installed extensions.
  virtual void ExecuteGet(Response* const response) OVERRIDE;

  // Installs an extension.
  virtual void ExecutePost(Response* const response) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionsCommand);
};

// Commands dealing with a particular extension.
class ExtensionCommand : public WebDriverCommand {
 public:
  ExtensionCommand(const std::vector<std::string>& path_segments,
                   const base::DictionaryValue* const parameters);
  virtual ~ExtensionCommand();

  virtual bool Init(Response* const response) OVERRIDE;

  virtual bool DoesGet() OVERRIDE;
  virtual bool DoesPost() OVERRIDE;
  virtual bool DoesDelete() OVERRIDE;

  // Returns information about the extension.
  virtual void ExecuteGet(Response* const response) OVERRIDE;

  // Modifies the extension.
  virtual void ExecutePost(Response* const response) OVERRIDE;

  // Uninstalls the extension.
  virtual void ExecuteDelete(Response* const response) OVERRIDE;

 private:
  std::string extension_id_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionCommand);
};

// Commands dealing with targeting non-tab views.
class ViewsCommand : public WebDriverCommand {
 public:
  ViewsCommand(const std::vector<std::string>& path_segments,
               const base::DictionaryValue* const parameters);
  virtual ~ViewsCommand();

  virtual bool DoesGet() OVERRIDE;

  // Returns all open views.
  virtual void ExecuteGet(Response* const response) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ViewsCommand);
};

#if !defined(NO_TCMALLOC) && (defined(OS_LINUX) || defined(OS_CHROMEOS))
class HeapProfilerDumpCommand : public WebDriverCommand {
 public:
  HeapProfilerDumpCommand(const std::vector<std::string>& path_segments,
                          const base::DictionaryValue* const parameters);
  virtual ~HeapProfilerDumpCommand();

  virtual bool DoesPost() OVERRIDE;
  virtual void ExecutePost(Response* const response) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(HeapProfilerDumpCommand);
};
#endif  // !defined(NO_TCMALLOC) && (defined(OS_LINUX) || defined(OS_CHROMEOS))

}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_COMMANDS_CHROME_COMMANDS_H_
