// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/test/test_web_ui.h"

namespace content {

TestWebUI::TestWebUI() : web_contents_(nullptr) {
}

TestWebUI::~TestWebUI() {
  ClearTrackedCalls();
}

void TestWebUI::ClearTrackedCalls() {
  call_data_.clear();
}

WebContents* TestWebUI::GetWebContents() const {
  return web_contents_;
}

WebUIController* TestWebUI::GetController() const {
  return nullptr;
}

float TestWebUI::GetDeviceScaleFactor() const {
  return 1.0f;
}

const base::string16& TestWebUI::GetOverriddenTitle() const {
  return temp_string_;
}

ui::PageTransition TestWebUI::GetLinkTransitionType() const {
  return ui::PAGE_TRANSITION_LINK;
}

int TestWebUI::GetBindings() const {
  return 0;
}

bool TestWebUI::HasRenderFrame() {
  return false;
}

void TestWebUI::AddMessageHandler(WebUIMessageHandler* handler) {
  handlers_.push_back(handler);
}

void TestWebUI::CallJavascriptFunction(const std::string& function_name) {
  call_data_.push_back(new CallData(function_name));
}

void TestWebUI::CallJavascriptFunction(const std::string& function_name,
                                       const base::Value& arg1) {
  call_data_.push_back(new CallData(function_name));
  call_data_.back()->TakeAsArg1(arg1.DeepCopy());
}

void TestWebUI::CallJavascriptFunction(const std::string& function_name,
                                       const base::Value& arg1,
                                       const base::Value& arg2) {
  call_data_.push_back(new CallData(function_name));
  call_data_.back()->TakeAsArg1(arg1.DeepCopy());
  call_data_.back()->TakeAsArg2(arg2.DeepCopy());
}

void TestWebUI::CallJavascriptFunction(const std::string& function_name,
                                       const base::Value& arg1,
                                       const base::Value& arg2,
                                       const base::Value& arg3) {
  call_data_.push_back(new CallData(function_name));
  call_data_.back()->TakeAsArg1(arg1.DeepCopy());
  call_data_.back()->TakeAsArg2(arg2.DeepCopy());
  call_data_.back()->TakeAsArg3(arg3.DeepCopy());
}

void TestWebUI::CallJavascriptFunction(const std::string& function_name,
                                       const base::Value& arg1,
                                       const base::Value& arg2,
                                       const base::Value& arg3,
                                       const base::Value& arg4) {
  NOTREACHED();
}

void TestWebUI::CallJavascriptFunction(
    const std::string& function_name,
    const std::vector<const base::Value*>& args) {
  NOTREACHED();
}

TestWebUI::CallData::CallData(const std::string& function_name)
    : function_name_(function_name) {
}

TestWebUI::CallData::~CallData() {
}

void TestWebUI::CallData::TakeAsArg1(base::Value* arg) {
  arg1_.reset(arg);
}

void TestWebUI::CallData::TakeAsArg2(base::Value* arg) {
  arg2_.reset(arg);
}

void TestWebUI::CallData::TakeAsArg3(base::Value* arg) {
  arg3_.reset(arg);
}

}  // namespace content
