// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_MAIN_PPAPI_INSTANCE2D_H_
#define PPAPI_MAIN_PPAPI_INSTANCE2D_H_

#include "ppapi/c/pp_instance.h"

#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/size.h"

#include "ppapi_main/ppapi_instance.h"


class PPAPIInstance2D : public PPAPIInstance {
 public:
  PPAPIInstance2D(PP_Instance instance, const char *args[]);
  virtual ~PPAPIInstance2D();

  // Called when we need to rebuild the context
  virtual void BuildContext(int32_t result, const pp::Size& new_size);

  // Called whenever a swap takes place
  virtual void Flushed(int result);

  virtual void Render(PP_Resource ctx, uint32_t width, uint32_t height);
  static PPAPIInstance2D* GetInstance2D();

 protected:
  pp::Graphics2D device_context_;
};


#endif  // PPAPI_MAIN_PPAPI_INSTANCE2D_H_
