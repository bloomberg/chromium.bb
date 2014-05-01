/*
 * Copyright (c) 2014 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * An application that doesn't define its own main but links in -lppapi
 * gets this one.  A plugin may instead have its own main that calls
 * PpapiPluginMain (or PpapiPluginStart) after doing some other setup.
 */

int PpapiPluginMain();

int main(void) {
  return PpapiPluginMain();
}
