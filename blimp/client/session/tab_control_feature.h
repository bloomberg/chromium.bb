// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_SESSION_TAB_CONTROL_FEATURE_H_
#define BLIMP_CLIENT_SESSION_TAB_CONTROL_FEATURE_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "blimp/client/blimp_client_export.h"
#include "blimp/net/blimp_message_processor.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
class Size;
}

namespace blimp {
namespace client {

class BLIMP_CLIENT_EXPORT TabControlFeature : public BlimpMessageProcessor {
 public:
  TabControlFeature();
  ~TabControlFeature() override;

  // Set the BlimpMessageProcessor that will be used to send
  // BlimpMessage::TAB_CONTROL messages to the engine.
  void set_outgoing_message_processor(
      scoped_ptr<BlimpMessageProcessor> processor);

  // Pushes the current size and scale information to the engine, which will
  // affect the web content display area for all tabs.
  void SetSizeAndScale(const gfx::Size& size, float device_pixel_ratio);

  void CreateTab(int tab_id);
  void CloseTab(int tab_id);

 private:
  // BlimpMessageProcessor implementation.
  void ProcessMessage(scoped_ptr<BlimpMessage> message,
                      const net::CompletionCallback& callback) override;

  // Used to send BlimpMessage::TAB_CONTROL messages to the engine.
  scoped_ptr<BlimpMessageProcessor> outgoing_message_processor_;

  // Used to avoid sending unnecessary messages to engine.
  gfx::Size last_size_;
  float last_device_pixel_ratio_ = -1;

  DISALLOW_COPY_AND_ASSIGN(TabControlFeature);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_SESSION_TAB_CONTROL_FEATURE_H_
