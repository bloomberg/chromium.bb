// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AURA_CHROME_BROWSER_MAIN_EXTRA_PARTS_AURA_H_
#define CHROME_BROWSER_UI_AURA_CHROME_BROWSER_MAIN_EXTRA_PARTS_AURA_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"

#if !defined(USE_ASH)
#include "base/memory/scoped_ptr.h"
#endif

namespace aura {
class DesktopStackingClient;
}

class ChromeBrowserMainExtraPartsAura : public ChromeBrowserMainExtraParts {
 public:
  ChromeBrowserMainExtraPartsAura();
  virtual ~ChromeBrowserMainExtraPartsAura();

  // Overridden from ChromeBrowserMainExtraParts:
  virtual void PreProfileInit() OVERRIDE;
  virtual void PostMainMessageLoopRun() OVERRIDE;

 private:
#if !defined(USE_ASH)
  scoped_ptr<aura::DesktopStackingClient> stacking_client_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainExtraPartsAura);
};

#endif  // CHROME_BROWSER_UI_AURA_CHROME_BROWSER_MAIN_EXTRA_PARTS_AURA_H_
