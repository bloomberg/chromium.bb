// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var vrShellVK = (function() {
  /** @const */ var VK_L_PANEL_LAYOUT = [
    [
      {'code': 'Key1', 'key': '1'},
      {'code': 'Key2', 'key': '2'},
      {'code': 'Key3', 'key': '3'},
    ],
    [
      {'code': 'Key4', 'key': '4'},
      {'code': 'Key5', 'key': '5'},
      {'code': 'Key6', 'key': '6'},
    ],
    [
      {'code': 'Key7', 'key': '7'},
      {'code': 'Key8', 'key': '8'},
      {'code': 'Key9', 'key': '9'},
    ],
    [
      {'code': 'KeyNegative', 'key': '-'},
      {'code': 'Key0', 'key': '0'},
      {'code': 'KeyPoint', 'key': '.'},
    ]
  ];

  /** @const */ var VK_R_PANEL_LAYOUT = [
    [
      {
        'code': 'Backspace',
        'key': 'Backspace',
        'image-class': 'vk-backspace-icon'
      },
    ],
    [
      {
        'code': 'Enter',
        'key': 'Enter',
        'height': 2.0,
        'text' : '\u{021B2}',
      },
    ],
    [
      {
        'code': 'Abort',
        'key': 'Escape',
        'image-class': 'vk-escape-icon',
        'action': vkHide
      },
    ],
  ];

  /** @const */ var VK_LAYOUTS = {
    'en-us-compact': {
      'levels': [
        [
          // Level 0: Unshifted.
          [
            {'code': 'KeyQ', 'key': 'q'},
            {'code': 'KeyW', 'key': 'w'},
            {'code': 'KeyE', 'key': 'e'},
            {'code': 'KeyR', 'key': 'r'},
            {'code': 'KeyT', 'key': 't'},
            {'code': 'KeyY', 'key': 'y'},
            {'code': 'KeyU', 'key': 'u'},
            {'code': 'KeyI', 'key': 'i'},
            {'code': 'KeyO', 'key': 'o'},
            {'code': 'KeyP', 'key': 'p'},
          ],
          [
            {'width': 0.5, 'display': 'spacer'},
            {'code': 'KeyA', 'key': 'a'},
            {'code': 'KeyS', 'key': 's'},
            {'code': 'KeyD', 'key': 'd'},
            {'code': 'KeyF', 'key': 'f'},
            {'code': 'KeyG', 'key': 'g'},
            {'code': 'KeyH', 'key': 'h'},
            {'code': 'KeyJ', 'key': 'j'},
            {'code': 'KeyK', 'key': 'k'},
            {'code': 'KeyL', 'key': 'l'},
          ],
          [
            {
              'code': 'ShiftLeft',
              'key': 'Shift',
              'text' : '\u{021E7}',
              'action': vkLevel,
              'level': 1
            },
            {'code': 'KeyZ', 'key': 'z'},
            {'code': 'KeyX', 'key': 'x'},
            {'code': 'KeyC', 'key': 'c'},
            {'code': 'KeyV', 'key': 'v'},
            {'code': 'KeyB', 'key': 'b'},
            {'code': 'KeyN', 'key': 'n'},
            {'code': 'KeyM', 'key': 'm'},
            {'code': '', 'key': '!'},
            {'code': '', 'key': '?'},
          ],
          [
            {
              'code': 'AltRight',
              'key': 'AltGraph',
              'text': '=\\<',
              'action': vkLevel,
              'level': 2
            },
            {'code': 'Slash', 'key': '/'},
            {
              'code': 'space',
              'key': ' ',
              'width': 6.00,
              'image-class': 'vk-space-icon'
            },
            {'code': 'Comma', 'key': ','},
            {'code': 'Period', 'key': '.'},
          ]
        ],
        [
          // Level 1: Shifted.
          [
            {'code': 'KeyQ', 'key': 'Q'},
            {'code': 'KeyW', 'key': 'W'},
            {'code': 'KeyE', 'key': 'E'},
            {'code': 'KeyR', 'key': 'R'},
            {'code': 'KeyT', 'key': 'T'},
            {'code': 'KeyY', 'key': 'Y'},
            {'code': 'KeyU', 'key': 'U'},
            {'code': 'KeyI', 'key': 'I'},
            {'code': 'KeyO', 'key': 'O'},
            {'code': 'KeyP', 'key': 'P'},
          ],
          [
            {'width': 0.5, 'display': 'spacer'},
            {'code': 'KeyA', 'key': 'A'},
            {'code': 'KeyS', 'key': 'S'},
            {'code': 'KeyD', 'key': 'D'},
            {'code': 'KeyF', 'key': 'F'},
            {'code': 'KeyG', 'key': 'G'},
            {'code': 'KeyH', 'key': 'H'},
            {'code': 'KeyJ', 'key': 'J'},
            {'code': 'KeyK', 'key': 'K'},
            {'code': 'KeyL', 'key': 'L'},
          ],
          [
            {
              'code': 'ShiftLeft',
              'key': 'Shift',
              'text' : '\u{021E7}',
              'action': vkLevel,
              'level': 0,
            },
            {'code': 'KeyZ', 'key': 'Z'},
            {'code': 'KeyX', 'key': 'X'},
            {'code': 'KeyC', 'key': 'C'},
            {'code': 'KeyV', 'key': 'V'},
            {'code': 'KeyB', 'key': 'B'},
            {'code': 'KeyN', 'key': 'N'},
            {'code': 'KeyM', 'key': 'M'},
            {'code': '', 'key': '!'},
            {'code': '', 'key': '?'},
          ],
          [
            {
              'code': 'AltRight',
              'key': 'AltGraph',
              'text': '=\\<',
              'action': vkLevel,
              'level': 2
            },
            {'code': 'Slash', 'key': '/'},
            {
              'code': 'space',
              'key': ' ',
              'width': 6.00,
              'image-class': 'vk-space-icon'
            },
            {'code': 'Comma', 'key': ','},
            {'code': 'Period', 'key': '.'},
          ]
        ],
        [
          // Level 2: Symbols.
          [
            {'code': 'KeyQ', 'key': '!'},
            {'code': 'KeyW', 'key': '@'},
            {'code': 'KeyE', 'key': '#'},
            {'code': 'KeyR', 'key': '$'},
            {'code': 'KeyT', 'key': '%'},
            {'code': 'KeyY', 'key': '^'},
            {'code': 'KeyU', 'key': '&'},
            {'code': 'KeyI', 'key': '*'},
            {'code': 'KeyO', 'key': '('},
            {'code': 'KeyP', 'key': ')'},
          ],
          [
            {'width': 0.5, 'display': 'spacer'},
            {'code': 'KeyA', 'key': '~'},
            {'code': 'KeyS', 'key': '`'},
            {'code': 'KeyD', 'key': '|'},
            {'code': 'KeyF', 'key': '{'},
            {'code': 'KeyG', 'key': '}'},
            {'code': 'KeyH', 'key': '['},
            {'code': 'KeyJ', 'key': ']'},
            {'code': 'KeyK', 'key': '-'},
            {'code': 'KeyL', 'key': '_'},
          ],
          [
            {'code': 'ShiftLeft', 'key': '/'},
            {'code': 'KeyZ', 'key': '\\'},
            {'code': 'KeyX', 'key': '+'},
            {'code': 'KeyC', 'key': '='},
            {'code': 'KeyV', 'key': ':'},
            {'code': 'KeyB', 'key': ';'},
            {'code': 'KeyN', 'key': '\''},
            {'code': 'KeyM', 'key': '<'},
            {'key': '>'},
            {'key': '"'},
          ],
          [
            {
              'code': 'AltRight',
              'key': 'AltGraph',
              'text': 'ABC',
              'action': vkLevel,
              'level': 0,
            },
            {'code': 'Slash', 'key': '/'},
            {
              'code': 'space',
              'key': ' ',
              'width': 6.00,
              'image-class': 'vk-space-icon'
            },
            {'code': 'Comma', 'key': ','},
            {'code': 'Period', 'key': '.'},
          ]
        ]
      ]
    }
  };

  /** @const */ var VK_BUTTON_SIZE = [64, 64, 'px'];

  var vkState = {'query': {'language': 'en-us', 'layout': 'compact'}};

  function vkHide(button) {
    // TODO(asimjour): Send a key event to support vkHide.
  }

  function vkLevel(button) {
    vkCh(button);
    vkActivateLevel(button.level);
  }

  function vkLevel0(button) {
    vkCh(button);
    vkActivateLevel(0);
  }

  function vkCh(button) {
    if (button.key || button.code) {
      // TODO(asimjour): Change the focus to the omnibox.
      if (button.code != 'AltRight')
        sendKey('key', button.code, button.key);
      if (vkState.level == 1)
        vkActivateLevel(0);
    }
  }

  function vkOnClick(e) {
    var button = e.vkButtonData;
    button.action(button);
  }

  function vkActivateLevel(n) {
    vkState.view.classList.remove('inputview-active-level-' + vkState.level);
    vkState.level = n;
    vkState.view.classList.add('inputview-active-level-' + vkState.level);
  }

  function vkNormalizeButtonData(buttonData) {
    if (!buttonData.text) {
      if (buttonData.key && !buttonData['image-class'])
        buttonData.text = buttonData.key;
      else
        buttonData.text = null;
    }
    buttonData.action = buttonData.action || vkCh;
    buttonData.width = buttonData.width || 1.00;
    buttonData.height = buttonData.height || 1.00;

    if (buttonData.display)
      buttonData.display = 'inputview-softkey-' + buttonData.display;
    else if (buttonData['image-class'])
      buttonData.display = 'inputview-softkey-0';
    else if (buttonData.text)
      buttonData.display = 'inputview-softkey-1';
    else
      buttonData.display = 'inputview-softkey-none';

    buttonData.styleWidth =
        (buttonData.width * VK_BUTTON_SIZE[0]) + VK_BUTTON_SIZE[2];
    buttonData.styleHeight =
        (buttonData.height * VK_BUTTON_SIZE[1]) + VK_BUTTON_SIZE[2];
    return buttonData;
  }

  function vkMkButton(index, buttonData) {
    var button = document.createElement('div');

    buttonData = vkNormalizeButtonData(buttonData);
    button.vkButtonData = buttonData
    button.classList.add('inputview-softkey-view');
    button.style.width = buttonData.styleWidth;
    button.style.height = buttonData.styleHeight;
    button.onclick = function() {
      vkOnClick(button);
    };

    var key = document.createElement('div');
    key.classList.add('inputview-softkey', buttonData.display);
    if (button.vkButtonData.code)
      key.classList.add('inputview-code-' + buttonData.code);
    var keyContent = key;
    if (button.vkButtonData['image-class']) {
      keyContent = document.createElement('div');
      keyContent.classList.add(button.vkButtonData['image-class'], 'vk-icon');
      key.appendChild(keyContent);
    }
    if (button.vkButtonData.class)
      keyContent.classList.add(button.vkButtonData.class);

    if (buttonData.text) {
      var charKey = document.createElement('div');
      charKey.classList.add('inputview-ch');
      charKey.textContent = buttonData.text;
      keyContent.appendChild(charKey);
    }

    button.appendChild(key);
    return button;
  }

  function vkMkRow(index, rowData) {
    var row = document.createElement('div');
    row.id = 'row' + index;
    row.classList.add('inputview-row');
    for (var c = 0; c < rowData.length; ++c)
      row.appendChild(vkMkButton(c, rowData[c]));
    return row;
  }

  function vkMkLevel(index, levelData) {
    var level = document.createElement('div');
    level.id = 'level' + index;
    level.classList.add('inputview-level', 'inputview-level-' + index);
    for (var r = 0; r < levelData.length; ++r)
      level.appendChild(vkMkRow(r, levelData[r]));
    return level;
  }

  function vkMkKb(view, layout) {
    vkState.view = view;
    vkState.layout = layout;
    vkState.levels = layout.levels.length;
    for (var key = 0; key < layout.levels.length; ++key)
      view.appendChild(vkMkLevel(key, vkState.layout.levels[key]));
  }

  function vkMkPanel(view, lPanelData) {
    for (var r = 0; r < lPanelData.length; ++r)
      view.appendChild(vkMkRow(r, lPanelData[r]));
  }

  function vkOnLoad() {
    // Build keyboard.
    vkState.layoutName = vkState.query.language + '-' + vkState.query.layout;
    vkMkKb(
        document.querySelector('#keyboardView'),
        VK_LAYOUTS[vkState.layoutName]);
    vkMkPanel(document.querySelector('#numberView'), VK_L_PANEL_LAYOUT);
    vkMkPanel(document.querySelector('#specialkeyView'), VK_R_PANEL_LAYOUT);

    vkActivateLevel(0);
  }

  // Flag values for ctrl, alt and shift as defined by EventFlags
  // in "event_constants.h".
  // @enum {number}
  var Modifier = {NONE: 0, ALT: 8, CONTROL: 4, SHIFT: 2};

  /** @const */ var DOM_KEYS = {
    'Backspace': 0x08,
    'Enter': 0x0D,
    'Escape': 0x1B,
    'AltGraph': 0x200103,
  };

  function domKeyValue(key) {
    if (key) {
      if (typeof key == 'number')
        return key;
      if (key.length == 1)
        return key.charCodeAt(0);
      if (DOM_KEYS.hasOwnProperty(key))
        return DOM_KEYS[key];
    }
    return 0;
  }

  /**
    * Dispatches a virtual key event. The system VK does not use the IME
    * API as its primary role is to work in conjunction with a non-VK aware
    * IME.
    */
  function sendKeyEvent(type, ch, code, name, modifiers) {
    var event = {
      type: type,
      charValue: ch,
      keyCode: code,
      keyName: name,
      modifiers: modifiers
    };
    api.doAction(api.Action.KEY_EVENT, event);
  }

  function sendKey(type, codeName, key) {
    sendKeyEvent(type, domKeyValue(key), 0, codeName, 0);
  }

  return {vkOnLoad: vkOnLoad};
})();

document.addEventListener('DOMContentLoaded', vrShellVK.vkOnLoad);
