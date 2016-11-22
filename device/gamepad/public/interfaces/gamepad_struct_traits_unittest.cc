// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "device/gamepad/public/interfaces/gamepad_struct_traits_test.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebGamepad.h"

namespace device {

namespace {

enum GamepadTestDataType {
  GamepadCommon = 0,
  GamepadPose_HasOrientation = 1,
  GamepadPose_HasPosition = 2,
  GamepadPose_Null = 3,
};

blink::WebGamepad GetWebGamepadInstance(GamepadTestDataType type) {
  blink::WebGamepadButton wgb(true, false, 1.0f);

  blink::WebGamepadVector wgv;
  memset(&wgv, 0, sizeof(blink::WebGamepadVector));
  wgv.notNull = true;
  wgv.x = wgv.y = wgv.z = 1.0f;

  blink::WebGamepadQuaternion wgq;
  memset(&wgq, 0, sizeof(blink::WebGamepadQuaternion));
  wgq.notNull = true;
  wgq.x = wgq.y = wgq.z = wgq.w = 2.0f;

  blink::WebGamepadPose wgp;
  memset(&wgp, 0, sizeof(blink::WebGamepadPose));
  if (type == GamepadPose_Null) {
    wgp.notNull = false;
  } else if (type == GamepadCommon) {
    wgp.notNull = wgp.hasOrientation = wgp.hasPosition = true;
    wgp.orientation = wgq;
    wgp.position = wgv;
    wgp.angularAcceleration = wgv;
  } else if (type == GamepadPose_HasOrientation) {
    wgp.notNull = wgp.hasOrientation = true;
    wgp.hasPosition = false;
    wgp.orientation = wgq;
    wgp.angularAcceleration = wgv;
  } else if (type == GamepadPose_HasPosition) {
    wgp.notNull = wgp.hasPosition = true;
    wgp.hasOrientation = false;
    wgp.position = wgv;
    wgp.angularAcceleration = wgv;
  }

  blink::WebUChar wch[blink::WebGamepad::mappingLengthCap] = {
      1,    8,    9,     127,   128,   1024,  1025,  1949,
      2047, 2048, 16383, 16384, 20000, 32767, 32768, 65535};

  blink::WebGamepad send;
  memset(&send, 0, sizeof(blink::WebGamepad));

  send.connected = true;
  for (size_t i = 0; i < blink::WebGamepad::mappingLengthCap; i++) {
    send.id[i] = send.mapping[i] = wch[i];
  }
  send.timestamp = 1234567890123456789ULL;
  send.axesLength = 0U;
  for (size_t i = 0; i < blink::WebGamepad::axesLengthCap; i++) {
    send.axesLength++;
    send.axes[i] = 1.0;
  }
  send.buttonsLength = 0U;
  for (size_t i = 0; i < blink::WebGamepad::buttonsLengthCap; i++) {
    send.buttonsLength++;
    send.buttons[i] = wgb;
  }
  send.pose = wgp;
  send.hand = blink::WebGamepadHand::GamepadHandRight;
  send.displayId = static_cast<unsigned short>(16);

  return send;
}

bool isWebGamepadButtonEqual(const blink::WebGamepadButton& lhs,
                             const blink::WebGamepadButton& rhs) {
  return (lhs.pressed == rhs.pressed && lhs.touched == rhs.touched &&
          lhs.value == rhs.value);
}

bool isWebGamepadVectorEqual(const blink::WebGamepadVector& lhs,
                             const blink::WebGamepadVector& rhs) {
  return ((lhs.notNull == false && rhs.notNull == false) ||
          (lhs.notNull == rhs.notNull && lhs.x == rhs.x && lhs.y == rhs.y &&
           lhs.z == rhs.z));
}

bool isWebGamepadQuaternionEqual(const blink::WebGamepadQuaternion& lhs,
                                 const blink::WebGamepadQuaternion& rhs) {
  return ((lhs.notNull == false && rhs.notNull == false) ||
          (lhs.notNull == rhs.notNull && lhs.x == rhs.x && lhs.y == rhs.y &&
           lhs.z == rhs.z && lhs.w == rhs.w));
}

bool isWebGamepadPoseEqual(const blink::WebGamepadPose& lhs,
                           const blink::WebGamepadPose& rhs) {
  if (lhs.notNull == false && rhs.notNull == false) {
    return true;
  }
  if (lhs.notNull != rhs.notNull || lhs.hasOrientation != rhs.hasOrientation ||
      lhs.hasPosition != rhs.hasPosition ||
      !isWebGamepadVectorEqual(lhs.angularVelocity, rhs.angularVelocity) ||
      !isWebGamepadVectorEqual(lhs.linearVelocity, rhs.linearVelocity) ||
      !isWebGamepadVectorEqual(lhs.angularAcceleration,
                               rhs.angularAcceleration) ||
      !isWebGamepadVectorEqual(lhs.linearAcceleration,
                               rhs.linearAcceleration)) {
    return false;
  }
  if (lhs.hasOrientation &&
      !isWebGamepadQuaternionEqual(lhs.orientation, rhs.orientation)) {
    return false;
  }
  if (lhs.hasPosition && !isWebGamepadVectorEqual(lhs.position, rhs.position)) {
    return false;
  }
  return true;
}

bool isWebGamepadEqual(const blink::WebGamepad& send,
                       const blink::WebGamepad& echo) {
  if (send.connected != echo.connected || send.timestamp != echo.timestamp ||
      send.axesLength != echo.axesLength ||
      send.buttonsLength != echo.buttonsLength ||
      !isWebGamepadPoseEqual(send.pose, echo.pose) || send.hand != echo.hand ||
      send.displayId != echo.displayId) {
    return false;
  }
  for (size_t i = 0; i < blink::WebGamepad::idLengthCap; i++) {
    if (send.id[i] != echo.id[i]) {
      return false;
    }
  }
  for (size_t i = 0; i < blink::WebGamepad::axesLengthCap; i++) {
    if (send.axes[i] != echo.axes[i]) {
      return false;
    }
  }
  for (size_t i = 0; i < blink::WebGamepad::buttonsLengthCap; i++) {
    if (!isWebGamepadButtonEqual(send.buttons[i], echo.buttons[i])) {
      return false;
    }
  }
  for (size_t i = 0; i < blink::WebGamepad::mappingLengthCap; i++) {
    if (send.mapping[i] != echo.mapping[i]) {
      return false;
    }
  }
  return true;
}

void ExpectWebGamepad(const blink::WebGamepad& send,
                      const base::Closure& closure,
                      const blink::WebGamepad& echo) {
  EXPECT_EQ(true, isWebGamepadEqual(send, echo));
  closure.Run();
}

}  // namespace

class GamepadStructTraitsTest : public testing::Test,
                                public device::mojom::GamepadStructTraitsTest {
 protected:
  GamepadStructTraitsTest() : binding_(this) {}

  void PassGamepad(const blink::WebGamepad& send,
                   const PassGamepadCallback& callback) override {
    callback.Run(send);
  }

  device::mojom::GamepadStructTraitsTestPtr GetGamepadStructTraitsTestProxy() {
    return binding_.CreateInterfacePtrAndBind();
  }

 private:
  base::MessageLoop message_loop_;
  mojo::Binding<device::mojom::GamepadStructTraitsTest> binding_;

  DISALLOW_COPY_AND_ASSIGN(GamepadStructTraitsTest);
};

TEST_F(GamepadStructTraitsTest, GamepadCommon) {
  blink::WebGamepad send = GetWebGamepadInstance(GamepadCommon);
  base::RunLoop loop;
  device::mojom::GamepadStructTraitsTestPtr proxy =
      GetGamepadStructTraitsTestProxy();
  proxy->PassGamepad(send,
                     base::Bind(&ExpectWebGamepad, send, loop.QuitClosure()));
  loop.Run();
}

TEST_F(GamepadStructTraitsTest, GamepadPose_HasOrientation) {
  blink::WebGamepad send = GetWebGamepadInstance(GamepadPose_HasOrientation);
  base::RunLoop loop;
  device::mojom::GamepadStructTraitsTestPtr proxy =
      GetGamepadStructTraitsTestProxy();
  proxy->PassGamepad(send,
                     base::Bind(&ExpectWebGamepad, send, loop.QuitClosure()));
  loop.Run();
}

TEST_F(GamepadStructTraitsTest, GamepadPose_HasPosition) {
  blink::WebGamepad send = GetWebGamepadInstance(GamepadPose_HasPosition);
  base::RunLoop loop;
  device::mojom::GamepadStructTraitsTestPtr proxy =
      GetGamepadStructTraitsTestProxy();
  proxy->PassGamepad(send,
                     base::Bind(&ExpectWebGamepad, send, loop.QuitClosure()));
  loop.Run();
}

TEST_F(GamepadStructTraitsTest, GamepadPose_Null) {
  blink::WebGamepad send = GetWebGamepadInstance(GamepadPose_Null);
  base::RunLoop loop;
  device::mojom::GamepadStructTraitsTestPtr proxy =
      GetGamepadStructTraitsTestProxy();
  proxy->PassGamepad(send,
                     base::Bind(&ExpectWebGamepad, send, loop.QuitClosure()));
  loop.Run();
}

}  // namespace device
