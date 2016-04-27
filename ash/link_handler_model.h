// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LINK_HANDLER_MODEL_H_
#define ASH_LINK_HANDLER_MODEL_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ui/gfx/image/image.h"

class GURL;

namespace ash {

struct ASH_EXPORT LinkHandlerInfo {
  std::string name;
  gfx::Image icon;
  uint32_t id;
};

// A model LinkHandlerModelFactory returns.
class ASH_EXPORT LinkHandlerModel {
 public:
  class Observer {
   public:
    virtual ~Observer();
    virtual void ModelChanged(const std::vector<LinkHandlerInfo>& handlers) = 0;
  };

  virtual ~LinkHandlerModel();
  virtual void AddObserver(Observer* observer) = 0;

  // Opens the |url| with a handler specified by the |handler_id|.
  virtual void OpenLinkWithHandler(const GURL& url, uint32_t handler_id) = 0;
};

}  // namespace ash

#endif  // ASH_LINK_HANDLER_MODEL_H_
