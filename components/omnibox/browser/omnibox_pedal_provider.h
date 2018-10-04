// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_PEDAL_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_PEDAL_PROVIDER_H_

#include <memory>
#include <vector>

#include "base/strings/utf_offset_string_conversions.h"

class OmniboxPedal;

class OmniboxPedalProvider {
 public:
  OmniboxPedalProvider();
  ~OmniboxPedalProvider();

  OmniboxPedal* FindPedalMatch(const base::string16& match_text) const;

 private:
  void Add(OmniboxPedal* pedal);
  void RegisterPedals();

  std::vector<std::unique_ptr<OmniboxPedal>> pedals_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxPedalProvider);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_PEDAL_PROVIDER_H_
