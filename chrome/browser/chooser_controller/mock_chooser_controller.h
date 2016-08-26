// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHOOSER_CONTROLLER_MOCK_CHOOSER_CONTROLLER_H_
#define CHROME_BROWSER_CHOOSER_CONTROLLER_MOCK_CHOOSER_CONTROLLER_H_

#include <vector>

#include "base/macros.h"
#include "chrome/browser/chooser_controller/chooser_controller.h"
#include "content/public/browser/bluetooth_chooser.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockChooserController : public ChooserController {
 public:
  explicit MockChooserController(content::RenderFrameHost* owner);
  ~MockChooserController() override;

  // ChooserController:
  bool ShouldShowIconBeforeText() const override;
  base::string16 GetNoOptionsText() const override;
  base::string16 GetOkButtonLabel() const override;
  size_t NumOptions() const override;
  int GetSignalStrengthLevel(size_t index) const override;
  base::string16 GetOption(size_t index) const override;
  base::string16 GetStatus() const override;
  MOCK_METHOD0(RefreshOptions, void());
  MOCK_METHOD1(Select, void(size_t index));
  MOCK_METHOD0(Cancel, void());
  MOCK_METHOD0(Close, void());
  MOCK_CONST_METHOD0(OpenHelpCenterUrl, void());

  void OnAdapterPresenceChanged(
      content::BluetoothChooser::AdapterPresence presence);
  void OnDiscoveryStateChanged(content::BluetoothChooser::DiscoveryState state);

  void OptionAdded(const base::string16& option_name,
                   int signal_strength_level);
  void OptionRemoved(const base::string16& option_name);
  void OptionUpdated(const base::string16& previous_option_name,
                     const base::string16& new_option_name,
                     int new_signal_strengh_level);

  static const int kNoImage;
  static const int kSignalStrengthLevel0Bar;
  static const int kSignalStrengthLevel1Bar;
  static const int kSignalStrengthLevel2Bar;
  static const int kSignalStrengthLevel3Bar;
  static const int kSignalStrengthLevel4Bar;

 private:
  void ClearAllOptions();

  struct OptionInfo {
    base::string16 name;
    int signal_strength_level;
  };

  std::vector<OptionInfo> options_;
  base::string16 no_options_text_;
  base::string16 status_text_;

  DISALLOW_COPY_AND_ASSIGN(MockChooserController);
};

#endif  // CHROME_BROWSER_CHOOSER_CONTROLLER_MOCK_CHOOSER_CONTROLLER_H_
