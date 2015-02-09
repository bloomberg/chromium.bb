// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/update_client.h"

#include "components/update_client/crx_update_item.h"

namespace update_client {

CrxUpdateItem::CrxUpdateItem()
    : status(kNew),
      unregistered(false),
      on_demand(false),
      diff_update_failed(false),
      error_category(0),
      error_code(0),
      extra_code1(0),
      diff_error_category(0),
      diff_error_code(0),
      diff_extra_code1(0) {
}

CrxUpdateItem::~CrxUpdateItem() {
}

CrxComponent::CrxComponent() : allow_background_download(true) {
}

CrxComponent::~CrxComponent() {
}

}  // namespace update_client
