// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.dynamicmodule;

interface IActivityDelegate {
  void onCreate(in Bundle savedInstanceState) = 0;

  void onPostCreate(in Bundle savedInstanceState) = 1;

  void onStart() = 2;

  void onStop() = 3;

  void onWindowFocusChanged(boolean hasFocus) = 4;

  void onSaveInstanceState(in Bundle outState) = 5;

  void onRestoreInstanceState(in Bundle savedInstanceState) = 6;

  void onResume() = 7;

  void onPause() = 8;

  void onDestroy() = 9;

  boolean onBackPressed() = 10;
}
