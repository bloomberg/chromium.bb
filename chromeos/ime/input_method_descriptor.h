// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_IME_INPUT_METHOD_DESCRIPTOR_H_
#define CHROMEOS_IME_INPUT_METHOD_DESCRIPTOR_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "chromeos/chromeos_export.h"

namespace chromeos {
namespace input_method {

class InputMethodWhitelist;

// A structure which represents an input method.
class CHROMEOS_EXPORT InputMethodDescriptor {
 public:
  InputMethodDescriptor();
  InputMethodDescriptor(const std::string& id,
                        const std::string& name,
                        const std::string& keyboard_layout,
                        const std::string& language_code,
                        const std::string& options_page_url);
  ~InputMethodDescriptor();

  bool operator==(const InputMethodDescriptor& other) const;
  bool operator!=(const InputMethodDescriptor& other) const;

  const std::string& id() const { return id_; }
  const std::string& name() const { return name_; }
  const std::string& keyboard_layout() const { return keyboard_layout_; }
  const std::string& language_code() const { return language_code_; }
  const std::string& options_page_url() const { return options_page_url_; }

 private:
  // An ID that identifies an input method engine (e.g., "t:latn-post",
  // "pinyin", "hangul").
  std::string id_;
  // A name used to specify the user-visible name of this input method.  It is
  // only used by extension IMEs, and should be blank for internal IMEs.
  std::string name_;
  // A preferred physical keyboard layout for the input method (e.g., "us",
  // "us(dvorak)", "jp"). Comma separated layout names do NOT appear.
  std::string keyboard_layout_;
  // Language code like "ko", "ja", "en-US", and "zh-CN".
  std::string language_code_;
  // Options page URL e.g.
  // "chrome-extension://ceaajjmckiakobniehbjpdcidfpohlin/options.html".
  // We can't use GURL here due to dependency policy. This field is valid only
  // for input method extension.
  std::string options_page_url_;
};

typedef std::vector<InputMethodDescriptor> InputMethodDescriptors;

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROMEOS_IME_INPUT_METHOD_DESCRIPTOR_H_
