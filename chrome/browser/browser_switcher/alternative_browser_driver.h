// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_SWITCHER_ALTERNATIVE_BROWSER_DRIVER_H_
#define CHROME_BROWSER_BROWSER_SWITCHER_ALTERNATIVE_BROWSER_DRIVER_H_

#include "base/macros.h"
#include "base/strings/string_piece_forward.h"

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

  // Updates the executable path that will be used for the browser when it is
  // launched. |path| is not necessarily a valid file path. It may be a
  // placeholder such as "${ie}".
  virtual void SetBrowserPath(base::StringPiece path) = 0;

  // Updates the command-line parameters to give to the browser when it is
  // launched.
  virtual void SetBrowserParameters(base::StringPiece parameters) = 0;

  // Tries to launch |browser| at the specified URL with DDE, using whatever
  // method is most appropriate.
  virtual bool TryLaunch(const GURL& url) = 0;
};

// Default concrete implementation for |AlternativeBrowserDriver|. This uses
// Windows primitives to access
class AlternativeBrowserDriverImpl : public AlternativeBrowserDriver {
 public:
  AlternativeBrowserDriverImpl();
  ~AlternativeBrowserDriverImpl() override;

  // AlternativeBrowserDriver
  void SetBrowserPath(base::StringPiece path) override;
  void SetBrowserParameters(base::StringPiece parameters) override;
  bool TryLaunch(const GURL& url) override;

 private:
  // Tries to launch |browser| at the specified URL with DDE, using whatever
  bool TryLaunchWithDde(const GURL& url);
  bool TryLaunchWithExec(const GURL& url);

  // Info on how to launch the currently-selected alternate browser.
  std::wstring browser_path_;
  std::wstring browser_params_;
  std::wstring dde_host_;

  DISALLOW_COPY_AND_ASSIGN(AlternativeBrowserDriverImpl);
};

}  // namespace browser_switcher

#endif  // CHROME_BROWSER_BROWSER_SWITCHER_ALTERNATIVE_BROWSER_DRIVER_H_
