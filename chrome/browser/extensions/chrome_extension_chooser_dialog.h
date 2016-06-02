// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_CHOOSER_DIALOG_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_CHOOSER_DIALOG_H_

#include "base/macros.h"
#include "build/build_config.h"

class ChooserController;

namespace content {
class WebContents;
}

class ChromeExtensionChooserDialog {
 public:
  explicit ChromeExtensionChooserDialog(content::WebContents* web_contents)
      : web_contents_(web_contents) {}
  ~ChromeExtensionChooserDialog() {}

  content::WebContents* web_contents() const { return web_contents_; }

// TODO(juncai): remove this preprocessor directive once the non-Mac
// implementation is done.
#if defined(OS_MACOSX)
  void ShowDialog(ChooserController* chooser_controller) const;
#endif  // defined(OS_MACOSX)

 private:
  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(ChromeExtensionChooserDialog);
};

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_CHOOSER_DIALOG_H_
