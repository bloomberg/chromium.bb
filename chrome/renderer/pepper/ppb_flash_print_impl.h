// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PEPPER_PPB_FLASH_PRINT_IMPL_H_
#define CHROME_RENDERER_PEPPER_PPB_FLASH_PRINT_IMPL_H_

struct PPB_Flash_Print_1_0;

class PPB_Flash_Print_Impl {
 public:
  static const PPB_Flash_Print_1_0* GetInterface();
};

#endif  // CHROME_RENDERER_PEPPER_PPB_FLASH_PRINT_IMPL_H_
