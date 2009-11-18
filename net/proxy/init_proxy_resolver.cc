// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/init_proxy_resolver.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "net/base/load_log.h"
#include "net/base/net_errors.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_resolver.h"
#include "net/proxy/proxy_script_fetcher.h"

namespace net {

InitProxyResolver::InitProxyResolver(ProxyResolver* resolver,
                                     ProxyScriptFetcher* proxy_script_fetcher)
    : resolver_(resolver),
      proxy_script_fetcher_(proxy_script_fetcher),
      ALLOW_THIS_IN_INITIALIZER_LIST(io_callback_(
          this, &InitProxyResolver::OnIOCompletion)),
      user_callback_(NULL),
      current_pac_url_index_(0u),
      next_state_(STATE_NONE) {
}

InitProxyResolver::~InitProxyResolver() {
  if (next_state_ != STATE_NONE)
    Cancel();
}

int InitProxyResolver::Init(const ProxyConfig& config,
                            CompletionCallback* callback,
                            LoadLog* load_log) {
  DCHECK_EQ(STATE_NONE, next_state_);
  DCHECK(callback);
  DCHECK(config.MayRequirePACResolver());
  DCHECK(!load_log_);

  load_log_ = load_log;

  LoadLog::BeginEvent(load_log_, LoadLog::TYPE_INIT_PROXY_RESOLVER);

  pac_urls_ = BuildPacUrlsFallbackList(config);
  DCHECK(!pac_urls_.empty());

  next_state_ = GetStartState();

  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    user_callback_ = callback;
  else
    DidCompleteInit();

  return rv;
}

// Initialize the fallback rules.
// (1) WPAD
// (2) Custom PAC URL.
InitProxyResolver::UrlList InitProxyResolver::BuildPacUrlsFallbackList(
    const ProxyConfig& config) const {
  UrlList pac_urls;
  if (config.auto_detect) {
     GURL pac_url = resolver_->expects_pac_bytes() ?
        GURL("http://wpad/wpad.dat") : GURL();
     pac_urls.push_back(pac_url);
  }
  if (config.pac_url.is_valid())
    pac_urls.push_back(config.pac_url);
  return pac_urls;
}

void InitProxyResolver::OnIOCompletion(int result) {
  DCHECK_NE(STATE_NONE, next_state_);
  int rv = DoLoop(result);
  if (rv != ERR_IO_PENDING) {
    DidCompleteInit();
    DoCallback(rv);
  }
}

int InitProxyResolver::DoLoop(int result) {
  DCHECK_NE(next_state_, STATE_NONE);
  int rv = result;
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_FETCH_PAC_SCRIPT:
        DCHECK_EQ(OK, rv);
        rv = DoFetchPacScript();
        break;
      case STATE_FETCH_PAC_SCRIPT_COMPLETE:
        rv = DoFetchPacScriptComplete(rv);
        break;
      case STATE_SET_PAC_SCRIPT:
        DCHECK_EQ(OK, rv);
        rv = DoSetPacScript();
        break;
      case STATE_SET_PAC_SCRIPT_COMPLETE:
        rv = DoSetPacScriptComplete(rv);
        break;
      default:
        NOTREACHED() << "bad state";
        rv = ERR_UNEXPECTED;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);
  return rv;
}

void InitProxyResolver::DoCallback(int result) {
  DCHECK_NE(ERR_IO_PENDING, result);
  DCHECK(user_callback_);
  user_callback_->Run(result);
}

int InitProxyResolver::DoFetchPacScript() {
  DCHECK(resolver_->expects_pac_bytes());

  LoadLog::BeginEvent(load_log_,
      LoadLog::TYPE_INIT_PROXY_RESOLVER_FETCH_PAC_SCRIPT);

  next_state_ = STATE_FETCH_PAC_SCRIPT_COMPLETE;

  const GURL& pac_url = current_pac_url();

  LOG(INFO) << "Starting fetch of PAC script " << pac_url;

  if (!proxy_script_fetcher_) {
    LOG(ERROR) << "Can't download PAC script, because no fetcher specified";
    return ERR_UNEXPECTED;
  }

  return proxy_script_fetcher_->Fetch(pac_url, &pac_bytes_, &io_callback_);
}

int InitProxyResolver::DoFetchPacScriptComplete(int result) {
  DCHECK(resolver_->expects_pac_bytes());

  LoadLog::EndEvent(load_log_,
      LoadLog::TYPE_INIT_PROXY_RESOLVER_FETCH_PAC_SCRIPT);

  LOG(INFO) << "Completed PAC script fetch of " << current_pac_url()
            << " with result " << ErrorToString(result)
            << ". Fetched a total of " << pac_bytes_.size() << " bytes";

  if (result != OK)
    return TryToFallbackPacUrl(result);

  next_state_ = STATE_SET_PAC_SCRIPT;
  return result;
}

int InitProxyResolver::DoSetPacScript() {
  LoadLog::BeginEvent(load_log_,
      LoadLog::TYPE_INIT_PROXY_RESOLVER_SET_PAC_SCRIPT);

  const GURL& pac_url = current_pac_url();

  next_state_ = STATE_SET_PAC_SCRIPT_COMPLETE;

  return resolver_->expects_pac_bytes() ?
      resolver_->SetPacScriptByData(pac_bytes_, &io_callback_) :
      resolver_->SetPacScriptByUrl(pac_url, &io_callback_);
}

int InitProxyResolver::DoSetPacScriptComplete(int result) {
  LoadLog::EndEvent(load_log_,
      LoadLog::TYPE_INIT_PROXY_RESOLVER_SET_PAC_SCRIPT);

  if (result != OK) {
    LOG(INFO) << "Failed configuring PAC using " << current_pac_url()
              << " with error " << ErrorToString(result);
    return TryToFallbackPacUrl(result);
  }
  return result;
}

int InitProxyResolver::TryToFallbackPacUrl(int error) {
  DCHECK_LT(error, 0);

  if (current_pac_url_index_ + 1 >= pac_urls_.size()) {
    // Nothing left to fall back to.
    return error;
  }

  // Advance to next URL in our list.
  ++current_pac_url_index_;

  LOG(INFO) << "Falling back to next PAC URL...";

  next_state_ = GetStartState();

  return OK;
}

InitProxyResolver::State InitProxyResolver::GetStartState() const {
  return resolver_->expects_pac_bytes() ?
      STATE_FETCH_PAC_SCRIPT : STATE_SET_PAC_SCRIPT;
}

const GURL& InitProxyResolver::current_pac_url() const {
  DCHECK_LT(current_pac_url_index_, pac_urls_.size());
  return pac_urls_[current_pac_url_index_];
}

void InitProxyResolver::DidCompleteInit() {
  LoadLog::EndEvent(load_log_, LoadLog::TYPE_INIT_PROXY_RESOLVER);
}

void InitProxyResolver::Cancel() {
  DCHECK_NE(STATE_NONE, next_state_);

  LoadLog::AddEvent(load_log_, LoadLog::TYPE_CANCELLED);

  switch (next_state_) {
    case STATE_FETCH_PAC_SCRIPT_COMPLETE:
      proxy_script_fetcher_->Cancel();
      break;
    case STATE_SET_PAC_SCRIPT_COMPLETE:
      resolver_->CancelSetPacScript();
      break;
    default:
      NOTREACHED();
      break;
  }

  DidCompleteInit();
}

}  // namespace net
