// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_SESSION_TAB_CONTROL_FEATURE_H_
#define BLIMP_CLIENT_SESSION_TAB_CONTROL_FEATURE_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "blimp/client/blimp_client_export.h"
#include "blimp/net/blimp_message_processor.h"

namespace gfx {
class Size;
}

namespace blimp {

class BLIMP_CLIENT_EXPORT TabControlFeature : public BlimpMessageProcessor {
 public:
  TabControlFeature();
  ~TabControlFeature() override;

  // Set the BlimpMessageProcessor that will be used to send
  // BlimpMessage::CONTROL messages to the engine.
  void set_outgoing_message_processor(
      scoped_ptr<BlimpMessageProcessor> processor);

  // Pushes the current size and scale information to the engine, which will
  // affect the web content display area for all tabs.
  void SetSizeAndScale(const gfx::Size& size, float device_pixel_ratio);

 private:
  // BlimpMessageProcessor implementation.
  void ProcessMessage(scoped_ptr<BlimpMessage> message,
                      const net::CompletionCallback& callback) override;

  // Used to send BlimpMessage::CONTROL messages to the engine.
  scoped_ptr<BlimpMessageProcessor> outgoing_message_processor_;

  DISALLOW_COPY_AND_ASSIGN(TabControlFeature);
};

}  // namespace blimp

#endif  // BLIMP_CLIENT_SESSION_TAB_CONTROL_FEATURE_H_
