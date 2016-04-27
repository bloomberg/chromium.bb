// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/link_handler_model_factory.h"

#include "ash/link_handler_model.h"

namespace ash {

LinkHandlerModelFactory::LinkHandlerModelFactory() {}
LinkHandlerModelFactory::~LinkHandlerModelFactory() {}

std::unique_ptr<LinkHandlerModel> LinkHandlerModelFactory::CreateModel(
    const GURL& url) {
  return nullptr;
}

}  // namespace ash
