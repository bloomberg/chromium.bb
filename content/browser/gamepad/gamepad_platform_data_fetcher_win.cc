// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gamepad/gamepad_platform_data_fetcher_win.h"

#include <dinput.h>
#include <dinputd.h>

#include "base/debug/trace_event.h"
#include "base/stringprintf.h"
#include "content/common/gamepad_messages.h"
#include "content/common/gamepad_hardware_buffer.h"

// This was removed from the Windows 8 SDK for some reason.
// We need it so we can get state for axes without worrying if they
// exist.
#ifndef DIDFT_OPTIONAL
#define DIDFT_OPTIONAL 0x80000000
#endif

namespace content {

using namespace WebKit;

namespace {

// See http://goo.gl/5VSJR. These are not available in all versions of the
// header, but they can be returned from the driver, so we define our own
// versions here.
static const BYTE kDeviceSubTypeGamepad = 1;
static const BYTE kDeviceSubTypeWheel = 2;
static const BYTE kDeviceSubTypeArcadeStick = 3;
static const BYTE kDeviceSubTypeFlightStick = 4;
static const BYTE kDeviceSubTypeDancePad = 5;
static const BYTE kDeviceSubTypeGuitar = 6;
static const BYTE kDeviceSubTypeGuitarAlternate = 7;
static const BYTE kDeviceSubTypeDrumKit = 8;
static const BYTE kDeviceSubTypeGuitarBass = 11;
static const BYTE kDeviceSubTypeArcadePad = 19;

float NormalizeXInputAxis(SHORT value) {
  return ((value + 32768.f) / 32767.5f) - 1.f;
}

const WebUChar* const GamepadSubTypeName(BYTE sub_type) {
  switch (sub_type) {
    case kDeviceSubTypeGamepad: return L"GAMEPAD";
    case kDeviceSubTypeWheel: return L"WHEEL";
    case kDeviceSubTypeArcadeStick: return L"ARCADE_STICK";
    case kDeviceSubTypeFlightStick: return L"FLIGHT_STICK";
    case kDeviceSubTypeDancePad: return L"DANCE_PAD";
    case kDeviceSubTypeGuitar: return L"GUITAR";
    case kDeviceSubTypeGuitarAlternate: return L"GUITAR_ALTERNATE";
    case kDeviceSubTypeDrumKit: return L"DRUM_KIT";
    case kDeviceSubTypeGuitarBass: return L"GUITAR_BASS";
    case kDeviceSubTypeArcadePad: return L"ARCADE_PAD";
    default: return L"<UNKNOWN>";
  }
}

bool GetDirectInputVendorProduct(IDirectInputDevice8* gamepad,
                                 std::string* vendor,
                                 std::string* product) {
  DIPROPDWORD prop;
  prop.diph.dwSize = sizeof(DIPROPDWORD);
  prop.diph.dwHeaderSize = sizeof(DIPROPHEADER);
  prop.diph.dwObj = 0;
  prop.diph.dwHow = DIPH_DEVICE;

  if (FAILED(gamepad->GetProperty(DIPROP_VIDPID, &prop.diph)))
    return false;
  *vendor = base::StringPrintf("%04x", LOWORD(prop.dwData));
  *product = base::StringPrintf("%04x", HIWORD(prop.dwData));
  return true;
}

// Sets the deadzone value for all axes of a gamepad.
// deadzone values range from 0 (no deadzone) to 10,000 (entire range
// is dead).
bool SetDirectInputDeadZone(IDirectInputDevice8* gamepad,
                            int deadzone) {
  DIPROPDWORD prop;
  prop.diph.dwSize = sizeof(DIPROPDWORD);
  prop.diph.dwHeaderSize = sizeof(DIPROPHEADER);
  prop.diph.dwObj = 0;
  prop.diph.dwHow = DIPH_DEVICE;
  prop.dwData = deadzone;
  return SUCCEEDED(gamepad->SetProperty(DIPROP_DEADZONE, &prop.diph));
}

struct InternalDirectInputDevice {
  IDirectInputDevice8* gamepad;
  GamepadStandardMappingFunction mapper;
  wchar_t id[WebGamepad::idLengthCap];
  GUID guid;
};

struct EnumDevicesContext {
  IDirectInput8* directinput_interface;
  std::vector<InternalDirectInputDevice>* directinput_devices;
};

// We define our own data format structure to attempt to get as many
// axes as possible.
struct JoyData {
  long axes[10];
  char buttons[24];
  DWORD pov;  // Often used for D-pads.
};

BOOL CALLBACK DirectInputEnumDevicesCallback(const DIDEVICEINSTANCE* instance,
                                             void* context) {
  EnumDevicesContext* ctxt = reinterpret_cast<EnumDevicesContext*>(context);
  IDirectInputDevice8* gamepad;

  if (FAILED(ctxt->directinput_interface->CreateDevice(instance->guidInstance,
                                                       &gamepad,
                                                       NULL)))
    return DIENUM_CONTINUE;

  gamepad->Acquire();

#define MAKE_AXIS(i) \
  {0, FIELD_OFFSET(JoyData, axes) + 4 * i, \
   DIDFT_AXIS | DIDFT_MAKEINSTANCE(i) | DIDFT_OPTIONAL, 0}
#define MAKE_BUTTON(i) \
  {&GUID_Button, FIELD_OFFSET(JoyData, buttons) + i, \
   DIDFT_BUTTON | DIDFT_MAKEINSTANCE(i) | DIDFT_OPTIONAL, 0}
#define MAKE_POV() \
  {&GUID_POV, FIELD_OFFSET(JoyData, pov), DIDFT_POV | DIDFT_OPTIONAL, 0}
  DIOBJECTDATAFORMAT rgodf[] = {
    MAKE_AXIS(0),
    MAKE_AXIS(1),
    MAKE_AXIS(2),
    MAKE_AXIS(3),
    MAKE_AXIS(4),
    MAKE_AXIS(5),
    MAKE_AXIS(6),
    MAKE_AXIS(7),
    MAKE_AXIS(8),
    MAKE_AXIS(9),
    MAKE_BUTTON(0),
    MAKE_BUTTON(1),
    MAKE_BUTTON(2),
    MAKE_BUTTON(3),
    MAKE_BUTTON(4),
    MAKE_BUTTON(5),
    MAKE_BUTTON(6),
    MAKE_BUTTON(7),
    MAKE_BUTTON(8),
    MAKE_BUTTON(9),
    MAKE_BUTTON(10),
    MAKE_BUTTON(11),
    MAKE_BUTTON(12),
    MAKE_BUTTON(13),
    MAKE_BUTTON(14),
    MAKE_BUTTON(15),
    MAKE_BUTTON(16),
    MAKE_POV(),
  };
#undef MAKE_AXIS
#undef MAKE_BUTTON
#undef MAKE_POV

  DIDATAFORMAT df = {
    sizeof (DIDATAFORMAT),
    sizeof (DIOBJECTDATAFORMAT),
    DIDF_ABSAXIS,
    sizeof (JoyData),
    sizeof (rgodf) / sizeof (rgodf[0]),
    rgodf
  };

  // If we can't set the data format on the device, don't add it to our
  // list, since we won't know how to read data from it.
  if (FAILED(gamepad->SetDataFormat(&df))) {
    gamepad->Release();
    return DIENUM_CONTINUE;
  }

  InternalDirectInputDevice device;
  device.guid = instance->guidInstance;
  device.gamepad = gamepad;
  std::string vendor;
  std::string product;
  if (!GetDirectInputVendorProduct(gamepad, &vendor, &product)) {
    gamepad->Release();
    return DIENUM_CONTINUE;
  }

  // Set the dead zone to 10% of the axis length for all axes. This
  // gives us a larger space for what's "neutral" so the controls don't
  // slowly drift.
  SetDirectInputDeadZone(gamepad, 1000);
  device.mapper = GetGamepadStandardMappingFunction(vendor, product);
  if (device.mapper) {
    base::swprintf(device.id,
                   WebGamepad::idLengthCap,
                   L"STANDARD GAMEPAD (%ls)",
                   instance->tszProductName);
    ctxt->directinput_devices->push_back(device);
  } else {
    gamepad->Release();
  }
  return DIENUM_CONTINUE;
}

}  // namespace

GamepadPlatformDataFetcherWin::GamepadPlatformDataFetcherWin()
    : xinput_dll_(base::FilePath(FILE_PATH_LITERAL("xinput1_3.dll"))),
      xinput_available_(GetXInputDllFunctions()) {
  directinput_available_ = SUCCEEDED(DirectInput8Create(
      GetModuleHandle(NULL),
      DIRECTINPUT_VERSION,
      IID_IDirectInput8,
      reinterpret_cast<void**>(&directinput_interface_),
      NULL));
  for (size_t i = 0; i < WebGamepads::itemsLengthCap; ++i)
    pad_state_[i].status = DISCONNECTED;
}

GamepadPlatformDataFetcherWin::~GamepadPlatformDataFetcherWin() {
  for (size_t i = 0; i < WebGamepads::itemsLengthCap; ++i) {
    if (pad_state_[i].status == DIRECTINPUT_CONNECTED)
      pad_state_[i].directinput_gamepad->Release();
  }
}

int GamepadPlatformDataFetcherWin::FirstAvailableGamepadId() const {
  for (size_t i = 0; i < WebGamepads::itemsLengthCap; ++i) {
    if (pad_state_[i].status == DISCONNECTED)
      return i;
  }
  return -1;
}

bool GamepadPlatformDataFetcherWin::HasXInputGamepad(int index) const {
  for (size_t i = 0; i < WebGamepads::itemsLengthCap; ++i) {
    if (pad_state_[i].status == XINPUT_CONNECTED &&
        pad_state_[i].xinput_index == index)
      return true;
  }
  return false;
}

bool GamepadPlatformDataFetcherWin::HasDirectInputGamepad(
    const GUID& guid) const {
  for (size_t i = 0; i < WebGamepads::itemsLengthCap; ++i) {
    if (pad_state_[i].status == DIRECTINPUT_CONNECTED &&
        pad_state_[i].guid == guid)
      return true;
  }
  return false;
}

void GamepadPlatformDataFetcherWin::EnumerateDevices(
    WebGamepads* pads) {
  TRACE_EVENT0("GAMEPAD", "EnumerateDevices");

  // Mark all disconnected pads DISCONNECTED.
  for (size_t i = 0; i < WebGamepads::itemsLengthCap; ++i) {
    if (!pads->items[i].connected)
      pad_state_[i].status = DISCONNECTED;
  }

  for (size_t i = 0; i < XUSER_MAX_COUNT; ++i) {
    if (HasXInputGamepad(i))
      continue;
    int pad_index = FirstAvailableGamepadId();
    if (pad_index == -1)
      return;  // We can't add any more gamepads.
    WebGamepad& pad = pads->items[pad_index];
    if (xinput_available_ && GetXInputPadConnectivity(i, &pad)) {
      pad_state_[pad_index].status = XINPUT_CONNECTED;
      pad_state_[pad_index].xinput_index = i;
    }
  }

  if (directinput_available_) {
    struct EnumDevicesContext context;
    std::vector<InternalDirectInputDevice> directinput_gamepads;
    context.directinput_interface = directinput_interface_;
    context.directinput_devices = &directinput_gamepads;

    directinput_interface_->EnumDevices(
        DI8DEVCLASS_GAMECTRL,
        &DirectInputEnumDevicesCallback,
        &context,
        DIEDFL_ATTACHEDONLY);
    for (size_t i = 0; i < directinput_gamepads.size(); ++i) {
      if (HasDirectInputGamepad(directinput_gamepads[i].guid)) {
        directinput_gamepads[i].gamepad->Release();
        continue;
      }
      int pad_index = FirstAvailableGamepadId();
      if (pad_index == -1)
        return;
      WebGamepad& pad = pads->items[pad_index];
      pad.connected = true;
      wcscpy_s(pad.id, WebGamepad::idLengthCap, directinput_gamepads[i].id);
      PadState& state = pad_state_[pad_index];
      state.status = DIRECTINPUT_CONNECTED;
      state.guid = directinput_gamepads[i].guid;
      state.directinput_gamepad = directinput_gamepads[i].gamepad;
      state.mapper = directinput_gamepads[i].mapper;
    }
  }
}


void GamepadPlatformDataFetcherWin::GetGamepadData(WebGamepads* pads,
                                                   bool devices_changed_hint) {
  TRACE_EVENT0("GAMEPAD", "GetGamepadData");

  if (!xinput_available_ && !directinput_available_) {
    pads->length = 0;
    return;
  }

  // A note on XInput devices:
  // If we got notification that system devices have been updated, then
  // run GetCapabilities to update the connected status and the device
  // identifier. It can be slow to do to both GetCapabilities and
  // GetState on unconnected devices, so we want to avoid a 2-5ms pause
  // here by only doing this when the devices are updated (despite
  // documentation claiming it's OK to call it any time).
  if (devices_changed_hint)
    EnumerateDevices(pads);

  for (size_t i = 0; i < WebGamepads::itemsLengthCap; ++i) {
    WebGamepad& pad = pads->items[i];
    if (pad_state_[i].status == XINPUT_CONNECTED)
      GetXInputPadData(i, &pad);
    else if (pad_state_[i].status == DIRECTINPUT_CONNECTED)
      GetDirectInputPadData(i, &pad);
  }
  pads->length = WebGamepads::itemsLengthCap;
}

bool GamepadPlatformDataFetcherWin::GetXInputPadConnectivity(
    int i,
    WebGamepad* pad) const {
  DCHECK(pad);
  TRACE_EVENT1("GAMEPAD", "GetXInputPadConnectivity", "id", i);
  XINPUT_CAPABILITIES caps;
  DWORD res = xinput_get_capabilities_(i, XINPUT_FLAG_GAMEPAD, &caps);
  if (res == ERROR_DEVICE_NOT_CONNECTED) {
    pad->connected = false;
    return false;
  } else {
    pad->connected = true;
    base::swprintf(pad->id,
                   WebGamepad::idLengthCap,
                   L"Xbox 360 Controller (XInput STANDARD %ls)",
                   GamepadSubTypeName(caps.SubType));
    return true;
  }
}

void GamepadPlatformDataFetcherWin::GetXInputPadData(
    int i,
    WebGamepad* pad) {
  // We rely on device_changed and GetCapabilities to tell us that
  // something's been connected, but we will mark as disconnected if
  // GetState returns that we've lost the pad.
  if (!pad->connected)
    return;

  XINPUT_STATE state;
  memset(&state, 0, sizeof(XINPUT_STATE));
  TRACE_EVENT_BEGIN1("GAMEPAD", "XInputGetState", "id", i);
  DWORD dwResult = xinput_get_state_(pad_state_[i].xinput_index, &state);
  TRACE_EVENT_END1("GAMEPAD", "XInputGetState", "id", i);

  if (dwResult == ERROR_SUCCESS) {
    pad->timestamp = state.dwPacketNumber;
    pad->buttonsLength = 0;
#define ADD(b) pad->buttons[pad->buttonsLength++] = \
  ((state.Gamepad.wButtons & (b)) ? 1.0 : 0.0);
    ADD(XINPUT_GAMEPAD_A);
    ADD(XINPUT_GAMEPAD_B);
    ADD(XINPUT_GAMEPAD_X);
    ADD(XINPUT_GAMEPAD_Y);
    ADD(XINPUT_GAMEPAD_LEFT_SHOULDER);
    ADD(XINPUT_GAMEPAD_RIGHT_SHOULDER);
    pad->buttons[pad->buttonsLength++] = state.Gamepad.bLeftTrigger / 255.0;
    pad->buttons[pad->buttonsLength++] = state.Gamepad.bRightTrigger / 255.0;
    ADD(XINPUT_GAMEPAD_BACK);
    ADD(XINPUT_GAMEPAD_START);
    ADD(XINPUT_GAMEPAD_LEFT_THUMB);
    ADD(XINPUT_GAMEPAD_RIGHT_THUMB);
    ADD(XINPUT_GAMEPAD_DPAD_UP);
    ADD(XINPUT_GAMEPAD_DPAD_DOWN);
    ADD(XINPUT_GAMEPAD_DPAD_LEFT);
    ADD(XINPUT_GAMEPAD_DPAD_RIGHT);
#undef ADD
    pad->axesLength = 0;
    // XInput are +up/+right, -down/-left, we want -up/-left.
    pad->axes[pad->axesLength++] = NormalizeXInputAxis(state.Gamepad.sThumbLX);
    pad->axes[pad->axesLength++] = -NormalizeXInputAxis(state.Gamepad.sThumbLY);
    pad->axes[pad->axesLength++] = NormalizeXInputAxis(state.Gamepad.sThumbRX);
    pad->axes[pad->axesLength++] = -NormalizeXInputAxis(state.Gamepad.sThumbRY);
  } else {
    pad->connected = false;
  }
}

void GamepadPlatformDataFetcherWin::GetDirectInputPadData(
    int index,
    WebGamepad* pad) {
  if (!pad->connected)
    return;

  IDirectInputDevice8* gamepad = pad_state_[index].directinput_gamepad;
  if (FAILED(gamepad->Poll())) {
    // Polling didn't work, try acquiring the gamepad.
    if (FAILED(gamepad->Acquire())) {
      pad->buttonsLength = 0;
      pad->axesLength = 0;
      return;
    }
    // Try polling again.
    if (FAILED(gamepad->Poll())) {
      pad->buttonsLength = 0;
      pad->axesLength = 0;
      return;
    }
  }
  JoyData state;
  if (FAILED(gamepad->GetDeviceState(sizeof(JoyData), &state))) {
    pad->connected = false;
    return;
  }

  WebGamepad raw;
  raw.connected = true;
  for (int i = 0; i < 16; i++)
    raw.buttons[i] = (state.buttons[i] & 0x80) ? 1.0 : 0.0;

  // We map the POV (often a D-pad) into the buttons 16-19.
  // DirectInput gives pov measurements in hundredths of degrees,
  // clockwise from "North".
  // We use 22.5 degree slices so we can handle diagonal D-raw presses.
  static const int arc_segment = 2250;  // 22.5 degrees = 1/16 circle
  if (state.pov > arc_segment && state.pov < 7 * arc_segment)
    raw.buttons[19] = 1.0;
  else
    raw.buttons[19] = 0.0;

  if (state.pov > 5 * arc_segment && state.pov < 11 * arc_segment)
    raw.buttons[17] = 1.0;
  else
    raw.buttons[17] = 0.0;

  if (state.pov > 9 * arc_segment && state.pov < 15 * arc_segment)
    raw.buttons[18] = 1.0;
  else
    raw.buttons[18] = 0.0;

  if (state.pov < 3 * arc_segment ||
      (state.pov > 13 * arc_segment && state.pov < 36000))
    raw.buttons[16] = 1.0;
  else
    raw.buttons[16] = 0.0;

  for (int i = 0; i < 10; i++)
    raw.axes[i] = state.axes[i];
  pad_state_[index].mapper(raw, pad);
}

bool GamepadPlatformDataFetcherWin::GetXInputDllFunctions() {
  xinput_get_capabilities_ = NULL;
  xinput_get_state_ = NULL;
  xinput_enable_ = static_cast<XInputEnableFunc>(
      xinput_dll_.GetFunctionPointer("XInputEnable"));
  if (!xinput_enable_)
    return false;
  xinput_get_capabilities_ = static_cast<XInputGetCapabilitiesFunc>(
      xinput_dll_.GetFunctionPointer("XInputGetCapabilities"));
  if (!xinput_get_capabilities_)
    return false;
  xinput_get_state_ = static_cast<XInputGetStateFunc>(
      xinput_dll_.GetFunctionPointer("XInputGetState"));
  if (!xinput_get_state_)
    return false;
  xinput_enable_(true);
  return true;
}

}  // namespace content
