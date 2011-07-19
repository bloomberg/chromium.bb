// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_HANDLE_WEB_KEYBOARD_EVENT_GTK_H_
#define CHROME_BROWSER_UI_VIEWS_HANDLE_WEB_KEYBOARD_EVENT_GTK_H_
#pragma once

namespace views {
class Widget;
}

struct NativeWebKeyboardEvent;

void HandleWebKeyboardEvent(views::Widget* widget,
                            const NativeWebKeyboardEvent& event);

#endif  // CHROME_BROWSER_UI_VIEWS_HANDLE_WEB_KEYBOARD_EVENT_GTK_H_
