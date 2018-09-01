// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/product.h"

#include "chrome/installer/util/browser_distribution.h"

namespace installer {

Product::Product(BrowserDistribution* distribution)
    : distribution_(distribution) {}

Product::~Product() {
}

}  // namespace installer
