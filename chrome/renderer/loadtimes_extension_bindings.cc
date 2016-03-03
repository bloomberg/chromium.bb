// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/loadtimes_extension_bindings.h"

#include <math.h>

#include "base/time/time.h"
#include "content/public/renderer/document_state.h"
#include "net/http/http_response_info.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "v8/include/v8.h"

using blink::WebDataSource;
using blink::WebLocalFrame;
using blink::WebNavigationType;
using content::DocumentState;

// Values for CSI "tran" property
const int kTransitionLink = 0;
const int kTransitionForwardBack = 6;
const int kTransitionOther = 15;
const int kTransitionReload = 16;

namespace extensions_v8 {

static const char* const kLoadTimesExtensionName = "v8/LoadTimes";

class LoadTimesExtensionWrapper : public v8::Extension {
 public:
  // Creates an extension which adds a new function, chromium.GetLoadTimes()
  // This function returns an object containing the following members:
  // requestTime: The time the request to load the page was received
  // loadTime: The time the renderer started the load process
  // finishDocumentLoadTime: The time the document itself was loaded
  //                         (this is before the onload() method is fired)
  // finishLoadTime: The time all loading is done, after the onload()
  //                 method and all resources
  // navigationType: A string describing what user action initiated the load
  LoadTimesExtensionWrapper() :
    v8::Extension(kLoadTimesExtensionName,
      "var chrome;"
      "if (!chrome)"
      "  chrome = {};"
      "chrome.loadTimes = function() {"
      "  native function GetLoadTimes();"
      "  return GetLoadTimes();"
      "};"
      "chrome.csi = function() {"
      "  native function GetCSI();"
      "  return GetCSI();"
      "}") {}

  v8::Local<v8::FunctionTemplate> GetNativeFunctionTemplate(
      v8::Isolate* isolate,
      v8::Local<v8::String> name) override {
    if (name->Equals(v8::String::NewFromUtf8(isolate, "GetLoadTimes"))) {
      return v8::FunctionTemplate::New(isolate, GetLoadTimes);
    } else if (name->Equals(v8::String::NewFromUtf8(isolate, "GetCSI"))) {
      return v8::FunctionTemplate::New(isolate, GetCSI);
    }
    return v8::Local<v8::FunctionTemplate>();
  }

  static const char* GetNavigationType(WebNavigationType nav_type) {
    switch (nav_type) {
      case blink::WebNavigationTypeLinkClicked:
        return "LinkClicked";
      case blink::WebNavigationTypeFormSubmitted:
        return "FormSubmitted";
      case blink::WebNavigationTypeBackForward:
        return "BackForward";
      case blink::WebNavigationTypeReload:
        return "Reload";
      case blink::WebNavigationTypeFormResubmitted:
        return "Resubmitted";
      case blink::WebNavigationTypeOther:
        return "Other";
    }
    return "";
  }

  static int GetCSITransitionType(WebNavigationType nav_type) {
    switch (nav_type) {
      case blink::WebNavigationTypeLinkClicked:
      case blink::WebNavigationTypeFormSubmitted:
      case blink::WebNavigationTypeFormResubmitted:
        return kTransitionLink;
      case blink::WebNavigationTypeBackForward:
        return kTransitionForwardBack;
      case blink::WebNavigationTypeReload:
        return kTransitionReload;
      case blink::WebNavigationTypeOther:
        return kTransitionOther;
    }
    return kTransitionOther;
  }

  static void GetLoadTimes(const v8::FunctionCallbackInfo<v8::Value>& args) {
    args.GetReturnValue().SetNull();
    WebLocalFrame* frame = WebLocalFrame::frameForCurrentContext();
    if (!frame) {
      return;
    }
    WebDataSource* data_source = frame->dataSource();
    if (!data_source) {
      return;
    }
    DocumentState* document_state = DocumentState::FromDataSource(data_source);
    if (!document_state) {
      return;
    }
    double request_time = document_state->request_time().ToDoubleT();
    double start_load_time = document_state->start_load_time().ToDoubleT();
    double commit_load_time = document_state->commit_load_time().ToDoubleT();
    double finish_document_load_time =
        document_state->finish_document_load_time().ToDoubleT();
    double finish_load_time = document_state->finish_load_time().ToDoubleT();
    double first_paint_time = document_state->first_paint_time().ToDoubleT();
    double first_paint_after_load_time =
        document_state->first_paint_after_load_time().ToDoubleT();
    std::string navigation_type =
        GetNavigationType(data_source->navigationType());
    bool was_fetched_via_spdy = document_state->was_fetched_via_spdy();
    bool was_npn_negotiated = document_state->was_npn_negotiated();
    std::string npn_negotiated_protocol =
        document_state->npn_negotiated_protocol();
    bool was_alternate_protocol_available =
        document_state->was_alternate_protocol_available();
    std::string connection_info = net::HttpResponseInfo::ConnectionInfoToString(
        document_state->connection_info());
    // Important: |frame|, |data_source| and |document_state| should not be
    // referred to below this line, as JS setters below can invalidate these
    // pointers.
    v8::Isolate* isolate = args.GetIsolate();
    v8::Local<v8::Context> ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> load_times = v8::Object::New(isolate);
    if (!load_times
             ->Set(ctx, v8::String::NewFromUtf8(isolate, "requestTime",
                                                v8::NewStringType::kNormal)
                            .ToLocalChecked(),
                   v8::Number::New(isolate, request_time))
             .FromMaybe(false)) {
      return;
    }

    if (!load_times
             ->Set(ctx, v8::String::NewFromUtf8(isolate, "startLoadTime",
                                                v8::NewStringType::kNormal)
                            .ToLocalChecked(),
                   v8::Number::New(isolate, start_load_time))
             .FromMaybe(false)) {
      return;
    }
    if (!load_times
             ->Set(ctx, v8::String::NewFromUtf8(isolate, "commitLoadTime",
                                                v8::NewStringType::kNormal)
                            .ToLocalChecked(),
                   v8::Number::New(isolate, commit_load_time))
             .FromMaybe(false)) {
      return;
    }
    if (!load_times
             ->Set(ctx,
                   v8::String::NewFromUtf8(isolate, "finishDocumentLoadTime",
                                           v8::NewStringType::kNormal)
                       .ToLocalChecked(),
                   v8::Number::New(isolate, finish_document_load_time))
             .FromMaybe(false)) {
      return;
    }
    if (!load_times
             ->Set(ctx, v8::String::NewFromUtf8(isolate, "finishLoadTime",
                                                v8::NewStringType::kNormal)
                            .ToLocalChecked(),
                   v8::Number::New(isolate, finish_load_time))
             .FromMaybe(false)) {
      return;
    }
    if (!load_times
             ->Set(ctx, v8::String::NewFromUtf8(isolate, "firstPaintTime",
                                                v8::NewStringType::kNormal)
                            .ToLocalChecked(),
                   v8::Number::New(isolate, first_paint_time))
             .FromMaybe(false)) {
      return;
    }
    if (!load_times
             ->Set(ctx,
                   v8::String::NewFromUtf8(isolate, "firstPaintAfterLoadTime",
                                           v8::NewStringType::kNormal)
                       .ToLocalChecked(),
                   v8::Number::New(isolate, first_paint_after_load_time))
             .FromMaybe(false)) {
      return;
    }
    if (!load_times
             ->Set(ctx, v8::String::NewFromUtf8(isolate, "navigationType",
                                                v8::NewStringType::kNormal)
                            .ToLocalChecked(),
                   v8::String::NewFromUtf8(isolate, navigation_type.c_str(),
                                           v8::NewStringType::kNormal)
                       .ToLocalChecked())
             .FromMaybe(false)) {
      return;
    }
    if (!load_times
             ->Set(ctx, v8::String::NewFromUtf8(isolate, "wasFetchedViaSpdy",
                                                v8::NewStringType::kNormal)
                            .ToLocalChecked(),
                   v8::Boolean::New(isolate, was_fetched_via_spdy))
             .FromMaybe(false)) {
      return;
    }
    if (!load_times
             ->Set(ctx, v8::String::NewFromUtf8(isolate, "wasNpnNegotiated",
                                                v8::NewStringType::kNormal)
                            .ToLocalChecked(),
                   v8::Boolean::New(isolate, was_npn_negotiated))
             .FromMaybe(false)) {
      return;
    }
    if (!load_times
             ->Set(ctx,
                   v8::String::NewFromUtf8(isolate, "npnNegotiatedProtocol",
                                           v8::NewStringType::kNormal)
                       .ToLocalChecked(),
                   v8::String::NewFromUtf8(isolate,
                                           npn_negotiated_protocol.c_str(),
                                           v8::NewStringType::kNormal)
                       .ToLocalChecked())
             .FromMaybe(false)) {
      return;
    }
    if (!load_times
             ->Set(ctx, v8::String::NewFromUtf8(isolate,
                                                "wasAlternateProtocolAvailable",
                                                v8::NewStringType::kNormal)
                            .ToLocalChecked(),
                   v8::Boolean::New(isolate, was_alternate_protocol_available))
             .FromMaybe(false)) {
      return;
    }
    if (!load_times
             ->Set(ctx, v8::String::NewFromUtf8(isolate, "connectionInfo",
                                                v8::NewStringType::kNormal)
                            .ToLocalChecked(),
                   v8::String::NewFromUtf8(isolate, connection_info.c_str(),
                                           v8::NewStringType::kNormal)
                       .ToLocalChecked())
             .FromMaybe(false)) {
      return;
    }
    args.GetReturnValue().Set(load_times);
  }

  static void GetCSI(const v8::FunctionCallbackInfo<v8::Value>& args) {
    args.GetReturnValue().SetNull();
    WebLocalFrame* frame = WebLocalFrame::frameForCurrentContext();
    if (!frame) {
      return;
    }
    WebDataSource* data_source = frame->dataSource();
    if (!data_source) {
      return;
    }
    DocumentState* document_state = DocumentState::FromDataSource(data_source);
    if (!document_state) {
      return;
    }
    base::Time now = base::Time::Now();
    base::Time start = document_state->request_time().is_null()
                           ? document_state->start_load_time()
                           : document_state->request_time();
    base::Time onload = document_state->finish_document_load_time();
    base::TimeDelta page = now - start;
    int navigation_type = GetCSITransitionType(data_source->navigationType());
    // Important: |frame|, |data_source| and |document_state| should not be
    // referred to below this line, as JS setters below can invalidate these
    // pointers.
    v8::Isolate* isolate = args.GetIsolate();
    v8::Local<v8::Context> ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> csi = v8::Object::New(isolate);
    if (!csi->Set(ctx, v8::String::NewFromUtf8(isolate, "startE",
                                               v8::NewStringType::kNormal)
                           .ToLocalChecked(),
                  v8::Number::New(isolate, floor(start.ToDoubleT() * 1000)))
             .FromMaybe(false)) {
      return;
    }
    if (!csi->Set(ctx, v8::String::NewFromUtf8(isolate, "onloadT",
                                               v8::NewStringType::kNormal)
                           .ToLocalChecked(),
                  v8::Number::New(isolate, floor(onload.ToDoubleT() * 1000)))
             .FromMaybe(false)) {
      return;
    }
    if (!csi->Set(ctx, v8::String::NewFromUtf8(isolate, "pageT",
                                               v8::NewStringType::kNormal)
                           .ToLocalChecked(),
                  v8::Number::New(isolate, page.InMillisecondsF()))
             .FromMaybe(false)) {
      return;
    }
    if (!csi->Set(ctx, v8::String::NewFromUtf8(isolate, "tran",
                                               v8::NewStringType::kNormal)
                           .ToLocalChecked(),
                  v8::Number::New(isolate, navigation_type))
             .FromMaybe(false)) {
      return;
    }
    args.GetReturnValue().Set(csi);
  }
};

v8::Extension* LoadTimesExtension::Get() {
  return new LoadTimesExtensionWrapper();
}

}  // namespace extensions_v8
