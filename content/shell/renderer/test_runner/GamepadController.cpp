// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/GamepadController.h"

#include "content/shell/renderer/test_runner/WebTestDelegate.h"

using namespace blink;

namespace WebTestRunner {

GamepadController::GamepadController()
{
    bindMethod("connect", &GamepadController::connect);
    bindMethod("disconnect", &GamepadController::disconnect);
    bindMethod("setId", &GamepadController::setId);
    bindMethod("setButtonCount", &GamepadController::setButtonCount);
    bindMethod("setButtonData", &GamepadController::setButtonData);
    bindMethod("setAxisCount", &GamepadController::setAxisCount);
    bindMethod("setAxisData", &GamepadController::setAxisData);

    bindFallbackMethod(&GamepadController::fallbackCallback);

    reset();
}

void GamepadController::bindToJavascript(WebFrame* frame, const WebString& classname)
{
    CppBoundClass::bindToJavascript(frame, classname);
}

void GamepadController::setDelegate(WebTestDelegate* delegate)
{
    m_delegate = delegate;
}

void GamepadController::reset()
{
    memset(&m_gamepads, 0, sizeof(m_gamepads));
}

void GamepadController::connect(const CppArgumentList& args, CppVariant* result)
{
    if (args.size() < 1) {
        m_delegate->printMessage("Invalid args");
        return;
    }
    int index = args[0].toInt32();
    if (index < 0 || index >= static_cast<int>(blink::WebGamepads::itemsLengthCap))
        return;
    m_gamepads.items[index].connected = true;
    m_gamepads.length = 0;
    for (unsigned i = 0; i < blink::WebGamepads::itemsLengthCap; ++i)
        if (m_gamepads.items[i].connected)
            m_gamepads.length = i + 1;
    m_delegate->setGamepadData(m_gamepads);
    result->setNull();
}

void GamepadController::disconnect(const CppArgumentList& args, CppVariant* result)
{
    if (args.size() < 1) {
        m_delegate->printMessage("Invalid args");
        return;
    }
    int index = args[0].toInt32();
    if (index < 0 || index >= static_cast<int>(blink::WebGamepads::itemsLengthCap))
        return;
    m_gamepads.items[index].connected = false;
    m_gamepads.length = 0;
    for (unsigned i = 0; i < blink::WebGamepads::itemsLengthCap; ++i)
        if (m_gamepads.items[i].connected)
            m_gamepads.length = i + 1;
    m_delegate->setGamepadData(m_gamepads);
    result->setNull();
}

void GamepadController::setId(const CppArgumentList& args, CppVariant* result)
{
    if (args.size() < 2) {
        m_delegate->printMessage("Invalid args");
        return;
    }
    int index = args[0].toInt32();
    if (index < 0 || index >= static_cast<int>(blink::WebGamepads::itemsLengthCap))
        return;
    std::string src = args[1].toString();
    const char* p = src.c_str();
    memset(m_gamepads.items[index].id, 0, sizeof(m_gamepads.items[index].id));
    for (unsigned i = 0; *p && i < blink::WebGamepad::idLengthCap - 1; ++i)
        m_gamepads.items[index].id[i] = *p++;
    m_delegate->setGamepadData(m_gamepads);
    result->setNull();
}

void GamepadController::setButtonCount(const CppArgumentList& args, CppVariant* result)
{
    if (args.size() < 2) {
        m_delegate->printMessage("Invalid args");
        return;
    }
    int index = args[0].toInt32();
    if (index < 0 || index >= static_cast<int>(blink::WebGamepads::itemsLengthCap))
        return;
    int buttons = args[1].toInt32();
    if (buttons < 0 || buttons >= static_cast<int>(blink::WebGamepad::buttonsLengthCap))
        return;
    m_gamepads.items[index].buttonsLength = buttons;
    m_delegate->setGamepadData(m_gamepads);
    result->setNull();
}

void GamepadController::setButtonData(const CppArgumentList& args, CppVariant* result)
{
    if (args.size() < 3) {
        m_delegate->printMessage("Invalid args");
        return;
    }
    int index = args[0].toInt32();
    if (index < 0 || index >= static_cast<int>(blink::WebGamepads::itemsLengthCap))
        return;
    int button = args[1].toInt32();
    if (button < 0 || button >= static_cast<int>(blink::WebGamepad::buttonsLengthCap))
        return;
    double data = args[2].toDouble();
    m_gamepads.items[index].buttons[button] = data;
    m_delegate->setGamepadData(m_gamepads);
    result->setNull();
}

void GamepadController::setAxisCount(const CppArgumentList& args, CppVariant* result)
{
    if (args.size() < 2) {
        m_delegate->printMessage("Invalid args");
        return;
    }
    int index = args[0].toInt32();
    if (index < 0 || index >= static_cast<int>(blink::WebGamepads::itemsLengthCap))
        return;
    int axes = args[1].toInt32();
    if (axes < 0 || axes >= static_cast<int>(blink::WebGamepad::axesLengthCap))
        return;
    m_gamepads.items[index].axesLength = axes;
    m_delegate->setGamepadData(m_gamepads);
    result->setNull();
}

void GamepadController::setAxisData(const CppArgumentList& args, CppVariant* result)
{
    if (args.size() < 3) {
        m_delegate->printMessage("Invalid args");
        return;
    }
    int index = args[0].toInt32();
    if (index < 0 || index >= static_cast<int>(blink::WebGamepads::itemsLengthCap))
        return;
    int axis = args[1].toInt32();
    if (axis < 0 || axis >= static_cast<int>(blink::WebGamepad::axesLengthCap))
        return;
    double data = args[2].toDouble();
    m_gamepads.items[index].axes[axis] = data;
    m_delegate->setGamepadData(m_gamepads);
    result->setNull();
}

void GamepadController::fallbackCallback(const CppArgumentList&, CppVariant* result)
{
    m_delegate->printMessage("CONSOLE MESSAGE: JavaScript ERROR: unknown method called on GamepadController\n");
    result->setNull();
}

}
