// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_BRIDGE_IMMERSIVE_HANDLER_FACTORY_MUS_H_
#define ASH_MUS_BRIDGE_IMMERSIVE_HANDLER_FACTORY_MUS_H_

#include "ash/shared/immersive_handler_factory.h"
#include "base/macros.h"

namespace ash {
namespace mus {

class ImmersiveHandlerFactoryMus : public ImmersiveHandlerFactory {
 public:
  ImmersiveHandlerFactoryMus();
  ~ImmersiveHandlerFactoryMus() override;

  // ImmersiveHandlerFactory:
  std::unique_ptr<ImmersiveFocusWatcher> CreateFocusWatcher(
      ImmersiveFullscreenController* controller) override;
  std::unique_ptr<ImmersiveGestureHandler> CreateGestureHandler(
      ImmersiveFullscreenController* controller) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ImmersiveHandlerFactoryMus);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_BRIDGE_IMMERSIVE_HANDLER_FACTORY_MUS_H_
