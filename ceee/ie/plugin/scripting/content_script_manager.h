// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// @file
// Content script manager declaration.

#ifndef CEEE_IE_PLUGIN_SCRIPTING_CONTENT_SCRIPT_MANAGER_H_
#define CEEE_IE_PLUGIN_SCRIPTING_CONTENT_SCRIPT_MANAGER_H_

#include <mshtml.h>
#include <string>
#include "base/basictypes.h"
#include "ceee/ie/plugin/scripting/script_host.h"
#include "chrome/common/extensions/user_script.h"
#include "googleurl/src/gurl.h"

// Forward declaration.
class IFrameEventHandlerHost;

// The content script manager implements CSS and content script injection
// for an extension in a frame.
// It also manages the ScriptHost, its bootstrapping and injecting the
// native API instance to the bootstrap JavaScript code.
class ContentScriptManager {
 public:
  ContentScriptManager();
  virtual ~ContentScriptManager();

  // Initialize before first use.
  // @param host the frame event handler host we delegate requests to.
  // @param require_all_frames Whether to require the all_frames property of the
  //        matched user scripts to be true.
  HRESULT Initialize(IFrameEventHandlerHost* host, bool require_all_frames);

  // Inject CSS for @p match_url into @p document
  // Needs to be invoked on READYSTATE_LOADED transition for @p document.
  virtual HRESULT LoadCss(const GURL& match_url, IHTMLDocument2* document);

  // Inject start scripts for @p match_url into @p document
  // Needs to be invoked on READYSTATE_LOADED transition for @p document.
  virtual HRESULT LoadStartScripts(const GURL& match_url,
                                   IHTMLDocument2* document);

  // Inject end scripts for @p match_url into @p document.
  // Needs to be invoked on READYSTATE_COMPLETE transition for @p document.
  virtual HRESULT LoadEndScripts(const GURL& match_url,
                                 IHTMLDocument2* document);

  // Run the given script code in the document. The @p file_path argument is
  // more debugging information, providing context for where the code came
  // from. If the given script code has a syntax or runtime error, this
  // function will still return S_OK since it is used to run third party code.
  virtual HRESULT ExecuteScript(const wchar_t* code,
                                const wchar_t* file_path,
                                IHTMLDocument2* document);

  // Inject the given CSS code into the document.
  virtual HRESULT InsertCss(const wchar_t* code, IHTMLDocument2* document);

  // Release any resources we've acquired or created.
  virtual HRESULT TearDown();

 protected:
  // Implementation of script loading Load{Start|End}Scripts.
  virtual HRESULT LoadScriptsImpl(const GURL& match_url,
                                  IHTMLDocument2* document,
                                  UserScript::RunLocation when);

  // Retrieves the script host, creating it if does not already exist.
  virtual HRESULT GetOrCreateScriptHost(IHTMLDocument2* document,
                                        IScriptHost** host);

  // Create a script host, virtual to make a unittest seam.
  virtual HRESULT CreateScriptHost(IScriptHost** host);

  // Load and initialize bootstrap scripts into host.
  virtual HRESULT BootstrapScriptHost(IScriptHost* host,
                                      IDispatch* api,
                                      const wchar_t* extension_id);

  // Initializes a newly-created script host.
  virtual HRESULT InitializeScriptHost(IHTMLDocument2* document,
                                       IScriptHost* host);

  // Testing seam.
  virtual HRESULT GetHeadNode(IHTMLDocument* document,
                              IHTMLDOMNode** head_node);
  virtual HRESULT InjectStyleTag(IHTMLDocument2* document,
                                 IHTMLDOMNode* head_node,
                                 const wchar_t* code);

  // The script engine that hosts the content scripts.
  CComPtr<IScriptHost> script_host_;

  // TODO(siggi@chromium.org): Stash the PageApi instance here for teardown.

  // This is where we get scripts and CSS to inject.
  CComPtr<IFrameEventHandlerHost> host_;

  // Whether to require all frames to be true when matching users scripts.
  bool require_all_frames_;

 private:
  // API accessible from javascript. This reference is required to release
  // some resources from ContentScriptManager::Teardown.
  // Must be instance of ContentScriptNativeApi.
  CComPtr<IDispatch> native_api_;
  DISALLOW_COPY_AND_ASSIGN(ContentScriptManager);
};

#endif  // CEEE_IE_PLUGIN_SCRIPTING_CONTENT_SCRIPT_MANAGER_H_
