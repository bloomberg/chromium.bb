// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_RENDER_VIEW_COMMANDS_H_
#define CHROME_COMMON_RENDER_VIEW_COMMANDS_H_
#pragma once

// These identify commands that the renderer process enables or disables
// in the browser process. For example, this is used on the Mac to keep
// the spell check menu commands in sync with the renderer state.
enum RenderViewCommand {
  RENDER_VIEW_COMMAND_TOGGLE_SPELL_CHECK,
};

enum RenderViewCommandCheckedState {
  RENDER_VIEW_COMMAND_CHECKED_STATE_UNCHECKED,
  RENDER_VIEW_COMMAND_CHECKED_STATE_CHECKED,
  RENDER_VIEW_COMMAND_CHECKED_STATE_MIXED,
};


#endif  // CHROME_COMMON_RENDER_VIEW_COMMANDS_H_
