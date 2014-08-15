// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/experience_sampling_private/experience_sampling.h"

#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/extensions/api/experience_sampling_private.h"
#include "extensions/browser/event_router.h"
#include "url/gurl.h"

namespace extensions {

// static
const char ExperienceSamplingEvent::kProceed[] = "proceed";
const char ExperienceSamplingEvent::kDeny[] = "deny";
const char ExperienceSamplingEvent::kCancel[] = "cancel";
const char ExperienceSamplingEvent::kReload[] = "reload";

// static
const char ExperienceSamplingEvent::kExtensionInstallDialog[] =
    "extension_install_dialog_";

// static
scoped_ptr<ExperienceSamplingEvent> ExperienceSamplingEvent::Create(
    const std::string& element_name,
    const GURL& destination,
    const GURL& referrer) {
  Profile* profile = NULL;
  if (g_browser_process->profile_manager())
     profile = g_browser_process->profile_manager()->GetLastUsedProfile();
  if (!profile)
    return scoped_ptr<ExperienceSamplingEvent>();
  return scoped_ptr<ExperienceSamplingEvent>(new ExperienceSamplingEvent(
      element_name, destination, referrer, profile));
}

// static
scoped_ptr<ExperienceSamplingEvent> ExperienceSamplingEvent::Create(
    const std::string& element_name) {
  return ExperienceSamplingEvent::Create(element_name, GURL(), GURL());
}

ExperienceSamplingEvent::ExperienceSamplingEvent(
    const std::string& element_name,
    const GURL& destination,
    const GURL& referrer,
    content::BrowserContext* browser_context)
    : has_viewed_details_(false),
      has_viewed_learn_more_(false),
      browser_context_(browser_context) {
  ui_element_.name = element_name;
  ui_element_.destination = destination.GetAsReferrer().possibly_invalid_spec();
  ui_element_.referrer = referrer.GetAsReferrer().possibly_invalid_spec();
  ui_element_.time = base::Time::Now().ToJsTime();
}

ExperienceSamplingEvent::~ExperienceSamplingEvent() {
}

void ExperienceSamplingEvent::CreateUserDecisionEvent(
    const std::string& decision_name) {
  // Check if this is from an incognito context. If it is, don't create and send
  // any events.
  if (browser_context_ && browser_context_->IsOffTheRecord())
    return;
  api::experience_sampling_private::UserDecision decision;
  decision.name = decision_name;
  decision.learn_more = has_viewed_learn_more();
  decision.details = has_viewed_details();
  decision.time = base::Time::Now().ToJsTime();

  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->Append(ui_element_.ToValue().release());
  args->Append(decision.ToValue().release());
  scoped_ptr<Event> event(new Event(
      api::experience_sampling_private::OnDecision::kEventName, args.Pass()));
  EventRouter* router = EventRouter::Get(browser_context_);
  if (router)
    router->BroadcastEvent(event.Pass());
}

void ExperienceSamplingEvent::CreateOnDisplayedEvent() {
  // Check if this is from an incognito context. If it is, don't create and send
  // any events.
  if (browser_context_ && browser_context_->IsOffTheRecord())
    return;
  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->Append(ui_element_.ToValue().release());
  scoped_ptr<Event> event(new Event(
      api::experience_sampling_private::OnDisplayed::kEventName, args.Pass()));
  EventRouter* router = EventRouter::Get(browser_context_);
  if (router)
    router->BroadcastEvent(event.Pass());
}

}  // namespace extensions
