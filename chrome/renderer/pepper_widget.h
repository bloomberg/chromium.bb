// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PEPPER_WIDGET_H_
#define CHROME_RENDERER_PEPPER_WIDGET_H_
#pragma once

#include "base/basictypes.h"
#include "third_party/npapi/bindings/npapi_extensions.h"

class Graphics2DDeviceContext;

// Every class that implements a Pepper widget derives from this.
class PepperWidget {
 public:
  static NPWidgetExtensions* GetWidgetExtensions();

  PepperWidget();
  void Init(NPP instance, int id);

  // Called as a result of the corresponding Pepper functions.
  virtual void Destroy() = 0;
  virtual void Paint(Graphics2DDeviceContext* context, const NPRect& dirty) = 0;
  virtual bool HandleEvent(const NPPepperEvent& event) = 0;
  virtual void GetProperty(NPWidgetProperty property, void* value) = 0;
  virtual void SetProperty(NPWidgetProperty property, void* value) = 0;

 protected:
  ~PepperWidget();

  // Tells the plugin that a property changed.
  void WidgetPropertyChanged(NPWidgetProperty property);

 private:
  NPP instance_;
  int id_;

  DISALLOW_COPY_AND_ASSIGN(PepperWidget);
};

#endif  // CHROME_RENDERER_PEPPER_WIDGET_H_
