// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/render_messages_params.h"

#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/render_messages.h"
#include "net/base/upload_data.h"

ViewMsg_Print_Params::ViewMsg_Print_Params()
    : margin_top(0),
      margin_left(0),
      dpi(0),
      min_shrink(0),
      max_shrink(0),
      desired_dpi(0),
      document_cookie(0),
      selection_only(false),
      supports_alpha_blend(true) {
}

ViewMsg_Print_Params::~ViewMsg_Print_Params() {
}

bool ViewMsg_Print_Params::Equals(const ViewMsg_Print_Params& rhs) const {
  return page_size == rhs.page_size &&
         printable_size == rhs.printable_size &&
         margin_top == rhs.margin_top &&
         margin_left == rhs.margin_left &&
         dpi == rhs.dpi &&
         min_shrink == rhs.min_shrink &&
         max_shrink == rhs.max_shrink &&
         desired_dpi == rhs.desired_dpi &&
         selection_only == rhs.selection_only &&
         supports_alpha_blend == rhs.supports_alpha_blend;
}

bool ViewMsg_Print_Params::IsEmpty() const {
  return !document_cookie && !desired_dpi && !max_shrink && !min_shrink &&
         !dpi && printable_size.IsEmpty() && !selection_only &&
         page_size.IsEmpty() && !margin_top && !margin_left &&
         !supports_alpha_blend;
}

ViewMsg_PrintPage_Params::ViewMsg_PrintPage_Params()
    : page_number(0) {
}

ViewMsg_PrintPage_Params::~ViewMsg_PrintPage_Params() {
}

ViewMsg_PrintPages_Params::ViewMsg_PrintPages_Params() {
}

ViewMsg_PrintPages_Params::~ViewMsg_PrintPages_Params() {
}

ViewHostMsg_DidPreviewDocument_Params::ViewHostMsg_DidPreviewDocument_Params()
    : data_size(0),
      document_cookie(0),
      expected_pages_count(0) {
#if defined(OS_WIN)
  // Initialize |metafile_data_handle| only on Windows because it maps
  // base::SharedMemoryHandle to HANDLE. We do not need to initialize this
  // variable on Posix because it maps base::SharedMemoryHandle to
  // FileDescriptior, which has the default constructor.
  metafile_data_handle = INVALID_HANDLE_VALUE;
#endif
}

ViewHostMsg_DidPreviewDocument_Params::
    ~ViewHostMsg_DidPreviewDocument_Params() {
}

ViewHostMsg_DidPrintPage_Params::ViewHostMsg_DidPrintPage_Params()
    : data_size(0),
      document_cookie(0),
      page_number(0),
      actual_shrink(0),
      has_visible_overlays(false) {
#if defined(OS_WIN)
    // Initialize |metafile_data_handle| only on Windows because it maps
  // base::SharedMemoryHandle to HANDLE. We do not need to initialize this
  // variable on Posix because it maps base::SharedMemoryHandle to
  // FileDescriptior, which has the default constructor.
  metafile_data_handle = INVALID_HANDLE_VALUE;
#endif
}

ViewHostMsg_DidPrintPage_Params::~ViewHostMsg_DidPrintPage_Params() {
}

ViewHostMsg_ScriptedPrint_Params::ViewHostMsg_ScriptedPrint_Params()
    : routing_id(0),
      host_window_id(0),
      cookie(0),
      expected_pages_count(0),
      has_selection(false),
      use_overlays(false) {
}

ViewHostMsg_ScriptedPrint_Params::~ViewHostMsg_ScriptedPrint_Params() {
}

ViewMsg_ExecuteCode_Params::ViewMsg_ExecuteCode_Params() {
}

ViewMsg_ExecuteCode_Params::ViewMsg_ExecuteCode_Params(
    int request_id,
    const std::string& extension_id,
    bool is_javascript,
    const std::string& code,
    bool all_frames)
    : request_id(request_id), extension_id(extension_id),
      is_javascript(is_javascript),
      code(code), all_frames(all_frames) {
}

ViewMsg_ExecuteCode_Params::~ViewMsg_ExecuteCode_Params() {
}

ViewHostMsg_DomMessage_Params::ViewHostMsg_DomMessage_Params()
    : request_id(0),
      has_callback(false),
      user_gesture(false) {
}

ViewHostMsg_DomMessage_Params::~ViewHostMsg_DomMessage_Params() {
}

ViewMsg_ExtensionLoaded_Params::ViewMsg_ExtensionLoaded_Params()
    : location(Extension::INVALID) {
}

ViewMsg_ExtensionLoaded_Params::~ViewMsg_ExtensionLoaded_Params() {
}

ViewMsg_ExtensionLoaded_Params::ViewMsg_ExtensionLoaded_Params(
    const ViewMsg_ExtensionLoaded_Params& other)
    : manifest(other.manifest->DeepCopy()),
      location(other.location),
      path(other.path),
      id(other.id) {
}

ViewMsg_ExtensionLoaded_Params::ViewMsg_ExtensionLoaded_Params(
    const Extension* extension)
    : manifest(new DictionaryValue()),
      location(extension->location()),
      path(extension->path()),
      id(extension->id()) {
  // As we need more bits of extension data in the renderer, add more keys to
  // this list.
  const char* kRendererExtensionKeys[] = {
    extension_manifest_keys::kPublicKey,
    extension_manifest_keys::kName,
    extension_manifest_keys::kVersion,
    extension_manifest_keys::kIcons,
    extension_manifest_keys::kPermissions,
    extension_manifest_keys::kApp
  };

  // Copy only the data we need.
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kRendererExtensionKeys); ++i) {
    Value* temp = NULL;
    if (extension->manifest_value()->Get(kRendererExtensionKeys[i], &temp))
      manifest->Set(kRendererExtensionKeys[i], temp->DeepCopy());
  }
}

scoped_refptr<Extension>
    ViewMsg_ExtensionLoaded_Params::ConvertToExtension() const {
  // Extensions that are loaded unpacked won't have a key.
  const bool kRequireKey = false;

  // The extension may have been loaded in a way that does not require
  // strict error checks to pass.  Do not do strict checks here.
  const bool kStrictErrorChecks = false;
  std::string error;

  scoped_refptr<Extension> extension(
      Extension::Create(path, location, *manifest, kRequireKey,
                        kStrictErrorChecks, &error));
  if (!extension.get())
    LOG(ERROR) << "Error deserializing extension: " << error;

  return extension;
}

namespace IPC {

template <>
struct ParamTraits<Extension::Location> {
  typedef Extension::Location param_type;
  static void Write(Message* m, const param_type& p) {
    int val = static_cast<int>(p);
    WriteParam(m, val);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int val = 0;
    if (!ReadParam(m, iter, &val) ||
        val < Extension::INVALID ||
        val >= Extension::NUM_LOCATIONS)
      return false;
    *p = static_cast<param_type>(val);
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    ParamTraits<int>::Log(static_cast<int>(p), l);
  }
};

void ParamTraits<ViewHostMsg_PageHasOSDD_Type>::Write(Message* m,
                                                      const param_type& p) {
  m->WriteInt(p.type);
}

bool ParamTraits<ViewHostMsg_PageHasOSDD_Type>::Read(const Message* m,
                                                     void** iter,
                                                     param_type* p) {
  int type;
  if (!m->ReadInt(iter, &type))
    return false;
  p->type = static_cast<param_type::Type>(type);
  return true;
}

void ParamTraits<ViewHostMsg_PageHasOSDD_Type>::Log(const param_type& p,
                                                    std::string* l) {
  std::string type;
  switch (p.type) {
    case ViewHostMsg_PageHasOSDD_Type::AUTODETECTED_PROVIDER:
      type = "ViewHostMsg_PageHasOSDD_Type::AUTODETECTED_PROVIDER";
      break;
    case ViewHostMsg_PageHasOSDD_Type::EXPLICIT_PROVIDER:
      type = "ViewHostMsg_PageHasOSDD_Type::EXPLICIT_PROVIDER";
      break;
    case ViewHostMsg_PageHasOSDD_Type::EXPLICIT_DEFAULT_PROVIDER:
      type = "ViewHostMsg_PageHasOSDD_Type::EXPLICIT_DEFAULT_PROVIDER";
      break;
    default:
      type = "UNKNOWN";
      break;
  }
  LogParam(type, l);
}

void ParamTraits<ViewHostMsg_GetSearchProviderInstallState_Params>::Write(
    Message* m, const param_type& p) {
  m->WriteInt(p.state);
}

bool ParamTraits<ViewHostMsg_GetSearchProviderInstallState_Params>::Read(
    const Message* m, void** iter, param_type* p) {
  int type;
  if (!m->ReadInt(iter, &type))
    return false;
  p->state = static_cast<param_type::State>(type);
  return true;
}

void ParamTraits<ViewHostMsg_GetSearchProviderInstallState_Params>::Log(
    const param_type& p, std::string* l) {
  std::string state;
  switch (p.state) {
    case ViewHostMsg_GetSearchProviderInstallState_Params::DENIED:
      state = "ViewHostMsg_GetSearchProviderInstallState_Params::DENIED";
      break;
    case ViewHostMsg_GetSearchProviderInstallState_Params::NOT_INSTALLED:
      state =
          "ViewHostMsg_GetSearchProviderInstallState_Params::NOT_INSTALLED";
      break;
    case ViewHostMsg_GetSearchProviderInstallState_Params::
        INSTALLED_BUT_NOT_DEFAULT:
      state = "ViewHostMsg_GetSearchProviderInstallState_Params::"
              "INSTALLED_BUT_NOT_DEFAULT";
      break;
    case ViewHostMsg_GetSearchProviderInstallState_Params::
        INSTALLED_AS_DEFAULT:
      state = "ViewHostMsg_GetSearchProviderInstallState_Params::"
              "INSTALLED_AS_DEFAULT";
      break;
    default:
      state = "UNKNOWN";
      break;
  }
  LogParam(state, l);
}

void ParamTraits<ViewMsg_Print_Params>::Write(Message* m, const param_type& p) {
  WriteParam(m, p.page_size);
  WriteParam(m, p.printable_size);
  WriteParam(m, p.margin_top);
  WriteParam(m, p.margin_left);
  WriteParam(m, p.dpi);
  WriteParam(m, p.min_shrink);
  WriteParam(m, p.max_shrink);
  WriteParam(m, p.desired_dpi);
  WriteParam(m, p.document_cookie);
  WriteParam(m, p.selection_only);
  WriteParam(m, p.supports_alpha_blend);
}

bool ParamTraits<ViewMsg_Print_Params>::Read(const Message* m,
                                             void** iter,
                                             param_type* p) {
  return ReadParam(m, iter, &p->page_size) &&
      ReadParam(m, iter, &p->printable_size) &&
      ReadParam(m, iter, &p->margin_top) &&
      ReadParam(m, iter, &p->margin_left) &&
      ReadParam(m, iter, &p->dpi) &&
      ReadParam(m, iter, &p->min_shrink) &&
      ReadParam(m, iter, &p->max_shrink) &&
      ReadParam(m, iter, &p->desired_dpi) &&
      ReadParam(m, iter, &p->document_cookie) &&
      ReadParam(m, iter, &p->selection_only) &&
      ReadParam(m, iter, &p->supports_alpha_blend);
}

void ParamTraits<ViewMsg_Print_Params>::Log(const param_type& p,
                                            std::string* l) {
  l->append("<ViewMsg_Print_Params>");
}

void ParamTraits<ViewMsg_PrintPage_Params>::Write(Message* m,
                                                  const param_type& p) {
  WriteParam(m, p.params);
  WriteParam(m, p.page_number);
}

bool ParamTraits<ViewMsg_PrintPage_Params>::Read(const Message* m,
                                                 void** iter,
                                                 param_type* p) {
  return ReadParam(m, iter, &p->params) &&
      ReadParam(m, iter, &p->page_number);
}

void ParamTraits<ViewMsg_PrintPage_Params>::Log(const param_type& p,
                                                std::string* l) {
  l->append("<ViewMsg_PrintPage_Params>");
}

void ParamTraits<ViewMsg_PrintPages_Params>::Write(Message* m,
                                                   const param_type& p) {
  WriteParam(m, p.params);
  WriteParam(m, p.pages);
}

bool ParamTraits<ViewMsg_PrintPages_Params>::Read(const Message* m,
                                                  void** iter,
                                                  param_type* p) {
  return ReadParam(m, iter, &p->params) &&
      ReadParam(m, iter, &p->pages);
}

void ParamTraits<ViewMsg_PrintPages_Params>::Log(const param_type& p,
                                                 std::string* l) {
  l->append("<ViewMsg_PrintPages_Params>");
}

void ParamTraits<ViewHostMsg_DidPreviewDocument_Params>::Write(Message* m,
                                                         const param_type& p) {
  WriteParam(m, p.metafile_data_handle);
  WriteParam(m, p.data_size);
  WriteParam(m, p.document_cookie);
  WriteParam(m, p.expected_pages_count);
}

bool ParamTraits<ViewHostMsg_DidPreviewDocument_Params>::Read(const Message* m,
                                                        void** iter,
                                                        param_type* p) {
  return ReadParam(m, iter, &p->metafile_data_handle) &&
      ReadParam(m, iter, &p->data_size) &&
      ReadParam(m, iter, &p->document_cookie) &&
      ReadParam(m, iter, &p->expected_pages_count);
}

void ParamTraits<ViewHostMsg_DidPreviewDocument_Params>::Log(
    const param_type& p, std::string* l) {
  l->append("<ViewHostMsg_DidPreviewDocument_Params>");
}

void ParamTraits<ViewHostMsg_DidPrintPage_Params>::Write(Message* m,
                                                         const param_type& p) {
  WriteParam(m, p.metafile_data_handle);
  WriteParam(m, p.data_size);
  WriteParam(m, p.document_cookie);
  WriteParam(m, p.page_number);
  WriteParam(m, p.actual_shrink);
  WriteParam(m, p.page_size);
  WriteParam(m, p.content_area);
  WriteParam(m, p.has_visible_overlays);
}

bool ParamTraits<ViewHostMsg_DidPrintPage_Params>::Read(const Message* m,
                                                        void** iter,
                                                        param_type* p) {
  return ReadParam(m, iter, &p->metafile_data_handle) &&
      ReadParam(m, iter, &p->data_size) &&
      ReadParam(m, iter, &p->document_cookie) &&
      ReadParam(m, iter, &p->page_number) &&
      ReadParam(m, iter, &p->actual_shrink) &&
      ReadParam(m, iter, &p->page_size) &&
      ReadParam(m, iter, &p->content_area) &&
      ReadParam(m, iter, &p->has_visible_overlays);
}

void ParamTraits<ViewHostMsg_DidPrintPage_Params>::Log(const param_type& p,
                                                       std::string* l) {
  l->append("<ViewHostMsg_DidPrintPage_Params>");
}

void ParamTraits<ViewHostMsg_ScriptedPrint_Params>::Write(Message* m,
                                                          const param_type& p) {
  WriteParam(m, p.routing_id);
  WriteParam(m, p.host_window_id);
  WriteParam(m, p.cookie);
  WriteParam(m, p.expected_pages_count);
  WriteParam(m, p.has_selection);
  WriteParam(m, p.use_overlays);
}

bool ParamTraits<ViewHostMsg_ScriptedPrint_Params>::Read(const Message* m,
                                                         void** iter,
                                                         param_type* p) {
  return
      ReadParam(m, iter, &p->routing_id) &&
      ReadParam(m, iter, &p->host_window_id) &&
      ReadParam(m, iter, &p->cookie) &&
      ReadParam(m, iter, &p->expected_pages_count) &&
      ReadParam(m, iter, &p->has_selection) &&
      ReadParam(m, iter, &p->use_overlays);
}

void ParamTraits<ViewHostMsg_ScriptedPrint_Params>::Log(const param_type& p,
                                                        std::string* l) {
  l->append("(");
  LogParam(p.routing_id, l);
  l->append(", ");
  LogParam(p.host_window_id, l);
  l->append(", ");
  LogParam(p.cookie, l);
  l->append(", ");
  LogParam(p.expected_pages_count, l);
  l->append(", ");
  LogParam(p.has_selection, l);
  l->append(",");
  LogParam(p.use_overlays, l);
  l->append(")");
}

void ParamTraits<ViewMsg_ExecuteCode_Params>::Write(Message* m,
                                                    const param_type& p) {
  WriteParam(m, p.request_id);
  WriteParam(m, p.extension_id);
  WriteParam(m, p.is_javascript);
  WriteParam(m, p.code);
  WriteParam(m, p.all_frames);
}

bool ParamTraits<ViewMsg_ExecuteCode_Params>::Read(const Message* m,
                                                   void** iter,
                                                   param_type* p) {
  return
      ReadParam(m, iter, &p->request_id) &&
      ReadParam(m, iter, &p->extension_id) &&
      ReadParam(m, iter, &p->is_javascript) &&
      ReadParam(m, iter, &p->code) &&
      ReadParam(m, iter, &p->all_frames);
}

void ParamTraits<ViewMsg_ExecuteCode_Params>::Log(const param_type& p,
                                                  std::string* l) {
  l->append("<ViewMsg_ExecuteCode_Params>");
}

void ParamTraits<ViewMsg_ExtensionLoaded_Params>::Write(Message* m,
                                                        const param_type& p) {
  WriteParam(m, p.location);
  WriteParam(m, p.path);
  WriteParam(m, *(p.manifest));
}

bool ParamTraits<ViewMsg_ExtensionLoaded_Params>::Read(const Message* m,
                                                       void** iter,
                                                       param_type* p) {
  p->manifest.reset(new DictionaryValue());
  return ReadParam(m, iter, &p->location) &&
         ReadParam(m, iter, &p->path) &&
         ReadParam(m, iter, p->manifest.get());
}

void ParamTraits<ViewMsg_ExtensionLoaded_Params>::Log(const param_type& p,
                                                      std::string* l) {
  l->append(p.id);
}

void ParamTraits<ViewHostMsg_DomMessage_Params>::Write(Message* m,
                                                       const param_type& p) {
  WriteParam(m, p.name);
  WriteParam(m, p.arguments);
  WriteParam(m, p.source_url);
  WriteParam(m, p.request_id);
  WriteParam(m, p.has_callback);
  WriteParam(m, p.user_gesture);
}

bool ParamTraits<ViewHostMsg_DomMessage_Params>::Read(const Message* m,
                                                      void** iter,
                                                      param_type* p) {
  return
      ReadParam(m, iter, &p->name) &&
      ReadParam(m, iter, &p->arguments) &&
      ReadParam(m, iter, &p->source_url) &&
      ReadParam(m, iter, &p->request_id) &&
      ReadParam(m, iter, &p->has_callback) &&
      ReadParam(m, iter, &p->user_gesture);
}

void ParamTraits<ViewHostMsg_DomMessage_Params>::Log(const param_type& p,
                                                     std::string* l) {
  l->append("(");
  LogParam(p.name, l);
  l->append(", ");
  LogParam(p.arguments, l);
  l->append(", ");
  LogParam(p.source_url, l);
  l->append(", ");
  LogParam(p.request_id, l);
  l->append(", ");
  LogParam(p.has_callback, l);
  l->append(", ");
  LogParam(p.user_gesture, l);
  l->append(")");
}

}  // namespace IPC
