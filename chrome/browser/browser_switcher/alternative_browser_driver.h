// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_SWITCHER_ALTERNATIVE_BROWSER_DRIVER_H_
#define CHROME_BROWSER_BROWSER_SWITCHER_ALTERNATIVE_BROWSER_DRIVER_H_

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/strings/string_piece_forward.h"
#include "base/values.h"
#include "build/build_config.h"

class GURL;

namespace browser_switcher {

// Interface for a helper class that does I/O operations for an
// |AlternativeBrowserLauncher|.
//
// - Reading from the Windows Registry
// - Communicating with an external process using the DDE protocol
// - Launching an external process
class AlternativeBrowserDriver {
 public:
  virtual ~AlternativeBrowserDriver();

  // TODO(nicolaso): Remove SetBrowserPath() and SetBrowserParameters().

  // Updates the executable path that will be used for the browser when it is
  // launched. |path| is not necessarily a valid file path. It may be a
  // placeholder such as "${ie}".
  virtual void SetBrowserPath(base::StringPiece path) = 0;

  // Updates the command-line parameters to give to the browser when it is
  // launched.
  virtual void SetBrowserParameters(const base::ListValue* parameters) = 0;

  // Expands environment variables and possibly tildes (~) in the given
  // string. On Windows, this should expand %VAR% to the var's value. On POSIX
  // systems, this should expand $VAR and ${VAR}, and expand ~ to the home
  // directory.
  //
  // "${url}" is special and is left as is in the string.
  virtual void ExpandEnvVars(std::string* str) const = 0;

  // If the string is a preset browser placeholder (e.g. ${ie}, ${firefox}),
  // expands it to an executable path. The list of preset browsers is
  // platform-specific.
  virtual void ExpandPresetBrowsers(std::string* str) const = 0;

  // Tries to launch |browser| at the specified URL, using whatever
  // method is most appropriate.
  virtual bool TryLaunch(const GURL& url) = 0;
};

// Default concrete implementation for |AlternativeBrowserDriver|. Uses a
// platform-specific method to locate and launch the appropriate browser.
class AlternativeBrowserDriverImpl : public AlternativeBrowserDriver {
 public:
  AlternativeBrowserDriverImpl();
  ~AlternativeBrowserDriverImpl() override;

  // AlternativeBrowserDriver
  void SetBrowserPath(base::StringPiece path) override;
  void SetBrowserParameters(const base::ListValue* parameters) override;
  void ExpandEnvVars(std::string* str) const override;
  void ExpandPresetBrowsers(std::string* str) const override;
  bool TryLaunch(const GURL& url) override;

  // Create the CommandLine object that would be used to launch an external
  // process.
  base::CommandLine CreateCommandLine(const GURL& url);

 private:
  using StringType = base::FilePath::StringType;

#if defined(OS_WIN)
  bool TryLaunchWithDde(const GURL& url);
  bool TryLaunchWithExec(const GURL& url);
#endif

  // Info on how to launch the currently-selected alternate browser.
  StringType browser_path_;
  std::vector<StringType> browser_params_;
#if defined(OS_WIN)
  StringType dde_host_;
#endif

  DISALLOW_COPY_AND_ASSIGN(AlternativeBrowserDriverImpl);
};

}  // namespace browser_switcher

#endif  // CHROME_BROWSER_BROWSER_SWITCHER_ALTERNATIVE_BROWSER_DRIVER_H_
