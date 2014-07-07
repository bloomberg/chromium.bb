// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/user_script_set.h"

#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/renderer/extensions_renderer_client.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_injection.h"
#include "extensions/renderer/user_script_injector.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "url/gurl.h"

namespace extensions {

namespace {

GURL GetDocumentUrlForFrame(blink::WebFrame* frame) {
  GURL data_source_url = ScriptContext::GetDataSourceURLForFrame(frame);
  if (!data_source_url.is_empty() && frame->isViewSourceModeEnabled()) {
    data_source_url = GURL(content::kViewSourceScheme + std::string(":") +
                           data_source_url.spec());
  }

  return data_source_url;
}

}  // namespace

UserScriptSet::UserScriptSet(const ExtensionSet* extensions)
    : extensions_(extensions) {
  content::RenderThread::Get()->AddObserver(this);
}

UserScriptSet::~UserScriptSet() {
}

void UserScriptSet::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void UserScriptSet::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void UserScriptSet::GetActiveExtensionIds(
    std::set<std::string>* ids) const {
  for (ScopedVector<UserScript>::const_iterator iter = scripts_.begin();
       iter != scripts_.end();
       ++iter) {
    DCHECK(!(*iter)->extension_id().empty());
    ids->insert((*iter)->extension_id());
  }
}

void UserScriptSet::GetInjections(
    ScopedVector<ScriptInjection>* injections,
    blink::WebFrame* web_frame,
    int tab_id,
    UserScript::RunLocation run_location) {
  GURL document_url = GetDocumentUrlForFrame(web_frame);
  for (ScopedVector<UserScript>::const_iterator iter = scripts_.begin();
       iter != scripts_.end();
       ++iter) {
    const Extension* extension = extensions_->GetByID((*iter)->extension_id());
    if (!extension)
      continue;
    scoped_ptr<ScriptInjection> injection = GetInjectionForScript(
        *iter, web_frame, tab_id, run_location, document_url, extension);
    if (injection.get())
      injections->push_back(injection.release());
  }
}

bool UserScriptSet::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(UserScriptSet, message)
    IPC_MESSAGE_HANDLER(ExtensionMsg_UpdateUserScripts, OnUpdateUserScripts)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void UserScriptSet::OnUpdateUserScripts(
    base::SharedMemoryHandle shared_memory,
    const std::set<std::string>& changed_extensions) {
  if (!base::SharedMemory::IsHandleValid(shared_memory)) {
    NOTREACHED() << "Bad scripts handle";
    return;
  }

  for (std::set<std::string>::const_iterator iter = changed_extensions.begin();
       iter != changed_extensions.end();
       ++iter) {
    if (!Extension::IdIsValid(*iter)) {
      NOTREACHED() << "Invalid extension id: " << *iter;
      return;
    }
  }

  if (UpdateScripts(shared_memory)) {
    FOR_EACH_OBSERVER(Observer,
                      observers_,
                      OnUserScriptsUpdated(changed_extensions, scripts_.get()));
  }
}

bool UserScriptSet::UpdateScripts(
    base::SharedMemoryHandle shared_memory) {
  bool only_inject_incognito =
      ExtensionsRendererClient::Get()->IsIncognitoProcess();

  // Create the shared memory object (read only).
  shared_memory_.reset(new base::SharedMemory(shared_memory, true));
  if (!shared_memory_.get())
    return false;

  // First get the size of the memory block.
  if (!shared_memory_->Map(sizeof(Pickle::Header)))
    return false;
  Pickle::Header* pickle_header =
      reinterpret_cast<Pickle::Header*>(shared_memory_->memory());

  // Now map in the rest of the block.
  int pickle_size = sizeof(Pickle::Header) + pickle_header->payload_size;
  shared_memory_->Unmap();
  if (!shared_memory_->Map(pickle_size))
    return false;

  // Unpickle scripts.
  uint64 num_scripts = 0;
  Pickle pickle(reinterpret_cast<char*>(shared_memory_->memory()), pickle_size);
  PickleIterator iter(pickle);
  CHECK(pickle.ReadUInt64(&iter, &num_scripts));

  scripts_.clear();
  scripts_.reserve(num_scripts);
  for (uint64 i = 0; i < num_scripts; ++i) {
    scoped_ptr<UserScript> script(new UserScript());
    script->Unpickle(pickle, &iter);

    // Note that this is a pointer into shared memory. We don't own it. It gets
    // cleared up when the last renderer or browser process drops their
    // reference to the shared memory.
    for (size_t j = 0; j < script->js_scripts().size(); ++j) {
      const char* body = NULL;
      int body_length = 0;
      CHECK(pickle.ReadData(&iter, &body, &body_length));
      script->js_scripts()[j].set_external_content(
          base::StringPiece(body, body_length));
    }
    for (size_t j = 0; j < script->css_scripts().size(); ++j) {
      const char* body = NULL;
      int body_length = 0;
      CHECK(pickle.ReadData(&iter, &body, &body_length));
      script->css_scripts()[j].set_external_content(
          base::StringPiece(body, body_length));
    }

    if (only_inject_incognito && !script->is_incognito_enabled())
      continue;  // This script shouldn't run in an incognito tab.

    scripts_.push_back(script.release());
  }

  return true;
}

scoped_ptr<ScriptInjection> UserScriptSet::GetInjectionForScript(
    UserScript* script,
    blink::WebFrame* web_frame,
    int tab_id,
    UserScript::RunLocation run_location,
    const GURL& document_url,
    const Extension* extension) {
  scoped_ptr<ScriptInjection> injection;
  if (web_frame->parent() && !script->match_all_frames())
    return injection.Pass();  // Only match subframes if the script declared it.

  GURL effective_document_url = ScriptContext::GetEffectiveDocumentURL(
      web_frame, document_url, script->match_about_blank());

  if (!script->MatchesURL(effective_document_url))
    return injection.Pass();

  if (extension->permissions_data()->GetContentScriptAccess(
          extension,
          effective_document_url,
          web_frame->top()->document().url(),
          -1,  // Content scripts are not tab-specific.
          -1,  // We don't have a process id in this context.
          NULL /* ignore error */) == PermissionsData::ACCESS_DENIED) {
    return injection.Pass();
  }

  bool inject_css = !script->css_scripts().empty() &&
                    run_location == UserScript::DOCUMENT_START;
  bool inject_js =
      !script->js_scripts().empty() && script->run_location() == run_location;
  if (inject_css || inject_js) {
    injection.reset(new ScriptInjection(
        scoped_ptr<ScriptInjector>(new UserScriptInjector(script, this)),
        web_frame,
        extension->id(),
        run_location,
        tab_id));
  }
  return injection.Pass();
}

}  // namespace extensions
