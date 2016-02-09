// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_DELEGATE_MAC_H_
#define CHROME_BROWSER_RESOURCE_DELEGATE_MAC_H_

#include "base/macros.h"
#include "ui/base/resource/resource_bundle.h"

// A resource bundle delegate for Mac. Primary purpose is to intercept certain
// resource ids, and to provide assets from system APIs for them at runtime.
class MacResourceDelegate : public ui::ResourceBundle::Delegate {
 public:
  MacResourceDelegate();
  ~MacResourceDelegate() override;

  // ui:ResourceBundle::Delegate implementation:
  base::FilePath GetPathForResourcePack(const base::FilePath& pack_path,
                                        ui::ScaleFactor scale_factor) override;
  base::FilePath GetPathForLocalePack(const base::FilePath& pack_path,
                                      const std::string& locale) override;
  gfx::Image GetImageNamed(int resource_id) override;
  gfx::Image GetNativeImageNamed(int resource_id) override;
  base::RefCountedStaticMemory* LoadDataResourceBytes(
      int resource_id,
      ui::ScaleFactor scale_factor) override;
  bool GetRawDataResource(int resource_id,
                          ui::ScaleFactor scale_factor,
                          base::StringPiece* value) override;
  bool GetLocalizedString(int message_id, base::string16* value) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MacResourceDelegate);
};

#endif  // CHROME_BROWSER_RESOURCE_DELEGATE_MAC_H_
