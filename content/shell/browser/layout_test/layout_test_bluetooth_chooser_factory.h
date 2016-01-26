// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_BLUETOOTH_CHOOSER_FACTORY_H_
#define CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_BLUETOOTH_CHOOSER_FACTORY_H_

#include "content/public/browser/bluetooth_chooser.h"

#include <set>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/bluetooth_chooser.h"

namespace url {
class Origin;
}

namespace content {

class WebContents;

class LayoutTestBluetoothChooserFactory {
 public:
  LayoutTestBluetoothChooserFactory();
  ~LayoutTestBluetoothChooserFactory();

  scoped_ptr<BluetoothChooser> RunBluetoothChooser(
      WebContents* web_contents,
      const BluetoothChooser::EventHandler& event_handler,
      const url::Origin& origin);

  std::vector<std::string> GetAndResetEvents();

  void SendEvent(BluetoothChooser::Event event, const std::string& device_id);

 private:
  class Chooser;

  std::vector<std::string> events_;

  // Contains the set of live choosers, in order to send them events.
  std::set<Chooser*> choosers_;

  base::WeakPtrFactory<LayoutTestBluetoothChooserFactory> weak_this_;
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_BLUETOOTH_CHOOSER_FACTORY_H_
