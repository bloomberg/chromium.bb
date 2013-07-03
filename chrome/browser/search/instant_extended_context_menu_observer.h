// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_INSTANT_EXTENDED_CONTEXT_MENU_OBSERVER_H_
#define CHROME_BROWSER_SEARCH_INSTANT_EXTENDED_CONTEXT_MENU_OBSERVER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/tab_contents/render_view_context_menu_observer.h"

namespace content {
class WebContents;
}

// This class disables menu items which perform poorly in instant extended mode.
class InstantExtendedContextMenuObserver
    : public RenderViewContextMenuObserver {
 public:
  explicit InstantExtendedContextMenuObserver(content::WebContents* contents);
  virtual ~InstantExtendedContextMenuObserver();

  // RenderViewContextMenuObserver implementation.
  virtual bool IsCommandIdSupported(int command_id) OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) OVERRIDE;

 private:
  // Whether the source web contents of the context menu corresponds to an
  // Instant overlay.
  const bool is_instant_overlay_;

  DISALLOW_COPY_AND_ASSIGN(InstantExtendedContextMenuObserver);
};

#endif  // CHROME_BROWSER_SEARCH_INSTANT_EXTENDED_CONTEXT_MENU_OBSERVER_H_
