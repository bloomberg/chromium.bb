// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/event_listener_map.h"

#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "extensions/browser/event_router.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::DictionaryValue;
using base::ListValue;
using base::StringValue;

namespace extensions {

namespace {

const char kExt1Id[] = "extension_1";
const char kExt2Id[] = "extension_2";
const char kEvent1Name[] = "event1";
const char kEvent2Name[] = "event2";

class EmptyDelegate : public EventListenerMap::Delegate {
  virtual void OnListenerAdded(const EventListener* listener) OVERRIDE {};
  virtual void OnListenerRemoved(const EventListener* listener) OVERRIDE {};
};

class EventListenerMapUnittest : public testing::Test {
 public:
  EventListenerMapUnittest()
    : delegate_(new EmptyDelegate),
      listeners_(new EventListenerMap(delegate_.get())),
      browser_context_(new content::TestBrowserContext),
      process_(new content::MockRenderProcessHost(browser_context_.get())) {
  }

  scoped_ptr<DictionaryValue> CreateHostSuffixFilter(
      const std::string& suffix) {
    scoped_ptr<DictionaryValue> filter(new DictionaryValue);
    scoped_ptr<ListValue> filter_list(new ListValue);
    scoped_ptr<DictionaryValue> filter_dict(new DictionaryValue);

    filter_dict->Set("hostSuffix", new StringValue(suffix));

    filter_list->Append(filter_dict.release());
    filter->Set("url", filter_list.release());
    return filter.Pass();
  }

  scoped_ptr<Event> CreateNamedEvent(const std::string& event_name) {
    return CreateEvent(event_name, GURL());
  }

  scoped_ptr<Event> CreateEvent(const std::string& event_name,
                                const GURL& url) {
    EventFilteringInfo info;
    info.SetURL(url);
    scoped_ptr<Event> result(new Event(event_name,
        make_scoped_ptr(new ListValue()), NULL, GURL(),
        EventRouter::USER_GESTURE_UNKNOWN, info));
    return result.Pass();
  }

 protected:
  scoped_ptr<EventListenerMap::Delegate> delegate_;
  scoped_ptr<EventListenerMap> listeners_;
  scoped_ptr<content::TestBrowserContext> browser_context_;
  scoped_ptr<content::MockRenderProcessHost> process_;
};

TEST_F(EventListenerMapUnittest, UnfilteredEventsGoToAllListeners) {
  listeners_->AddListener(scoped_ptr<EventListener>(new EventListener(
      kEvent1Name, kExt1Id, NULL, scoped_ptr<DictionaryValue>())));

  scoped_ptr<Event> event(CreateNamedEvent(kEvent1Name));
  std::set<const EventListener*> targets(listeners_->GetEventListeners(*event));
  ASSERT_EQ(1u, targets.size());
}

TEST_F(EventListenerMapUnittest, FilteredEventsGoToAllMatchingListeners) {
  listeners_->AddListener(scoped_ptr<EventListener>(new EventListener(
      kEvent1Name, kExt1Id, NULL, CreateHostSuffixFilter("google.com"))));
  listeners_->AddListener(scoped_ptr<EventListener>(new EventListener(
      kEvent1Name, kExt1Id, NULL, scoped_ptr<DictionaryValue>(
      new DictionaryValue))));

  scoped_ptr<Event> event(CreateNamedEvent(kEvent1Name));
  event->filter_info.SetURL(GURL("http://www.google.com"));
  std::set<const EventListener*> targets(listeners_->GetEventListeners(*event));
  ASSERT_EQ(2u, targets.size());
}

TEST_F(EventListenerMapUnittest, FilteredEventsOnlyGoToMatchingListeners) {
  listeners_->AddListener(scoped_ptr<EventListener>(new EventListener(
      kEvent1Name, kExt1Id, NULL, CreateHostSuffixFilter("google.com"))));
  listeners_->AddListener(scoped_ptr<EventListener>(new EventListener(
      kEvent1Name, kExt1Id, NULL, CreateHostSuffixFilter("yahoo.com"))));

  scoped_ptr<Event> event(CreateNamedEvent(kEvent1Name));
  event->filter_info.SetURL(GURL("http://www.google.com"));
  std::set<const EventListener*> targets(listeners_->GetEventListeners(*event));
  ASSERT_EQ(1u, targets.size());
}

TEST_F(EventListenerMapUnittest, LazyAndUnlazyListenersGetReturned) {
  listeners_->AddListener(scoped_ptr<EventListener>(new EventListener(
      kEvent1Name, kExt1Id, NULL, CreateHostSuffixFilter("google.com"))));

  listeners_->AddListener(scoped_ptr<EventListener>(new EventListener(
      kEvent1Name, kExt1Id, process_.get(),
      CreateHostSuffixFilter("google.com"))));

  scoped_ptr<Event> event(CreateNamedEvent(kEvent1Name));
  event->filter_info.SetURL(GURL("http://www.google.com"));
  std::set<const EventListener*> targets(listeners_->GetEventListeners(*event));
  ASSERT_EQ(2u, targets.size());
}

TEST_F(EventListenerMapUnittest, TestRemovingByProcess) {
  listeners_->AddListener(scoped_ptr<EventListener>(new EventListener(
      kEvent1Name, kExt1Id, NULL, CreateHostSuffixFilter("google.com"))));

  listeners_->AddListener(scoped_ptr<EventListener>(new EventListener(
      kEvent1Name, kExt1Id, process_.get(),
      CreateHostSuffixFilter("google.com"))));

  listeners_->RemoveListenersForProcess(process_.get());

  scoped_ptr<Event> event(CreateNamedEvent(kEvent1Name));
  event->filter_info.SetURL(GURL("http://www.google.com"));
  std::set<const EventListener*> targets(listeners_->GetEventListeners(*event));
  ASSERT_EQ(1u, targets.size());
}

TEST_F(EventListenerMapUnittest, TestRemovingByListener) {
  listeners_->AddListener(scoped_ptr<EventListener>(new EventListener(
      kEvent1Name, kExt1Id, NULL, CreateHostSuffixFilter("google.com"))));

  listeners_->AddListener(scoped_ptr<EventListener>(new EventListener(
      kEvent1Name, kExt1Id, process_.get(),
      CreateHostSuffixFilter("google.com"))));

  scoped_ptr<EventListener> listener(new EventListener(kEvent1Name, kExt1Id,
      process_.get(), CreateHostSuffixFilter("google.com")));
  listeners_->RemoveListener(listener.get());

  scoped_ptr<Event> event(CreateNamedEvent(kEvent1Name));
  event->filter_info.SetURL(GURL("http://www.google.com"));
  std::set<const EventListener*> targets(listeners_->GetEventListeners(*event));
  ASSERT_EQ(1u, targets.size());
}

TEST_F(EventListenerMapUnittest, TestLazyDoubleAddIsUndoneByRemove) {
  listeners_->AddListener(scoped_ptr<EventListener>(new EventListener(
      kEvent1Name, kExt1Id, NULL, CreateHostSuffixFilter("google.com"))));
  listeners_->AddListener(scoped_ptr<EventListener>(new EventListener(
      kEvent1Name, kExt1Id, NULL, CreateHostSuffixFilter("google.com"))));

  scoped_ptr<EventListener> listener(new EventListener(
        kEvent1Name, kExt1Id, NULL, CreateHostSuffixFilter("google.com")));
  listeners_->RemoveListener(listener.get());

  scoped_ptr<Event> event(CreateNamedEvent(kEvent1Name));
  event->filter_info.SetURL(GURL("http://www.google.com"));
  std::set<const EventListener*> targets(listeners_->GetEventListeners(*event));
  ASSERT_EQ(0u, targets.size());
}

TEST_F(EventListenerMapUnittest, HostSuffixFilterEquality) {
  scoped_ptr<DictionaryValue> filter1(CreateHostSuffixFilter("google.com"));
  scoped_ptr<DictionaryValue> filter2(CreateHostSuffixFilter("google.com"));
  ASSERT_TRUE(filter1->Equals(filter2.get()));
}

TEST_F(EventListenerMapUnittest, RemoveLazyListenersForExtension) {
  listeners_->AddListener(scoped_ptr<EventListener>(new EventListener(
      kEvent1Name, kExt1Id, NULL, CreateHostSuffixFilter("google.com"))));
  listeners_->AddListener(scoped_ptr<EventListener>(new EventListener(
      kEvent2Name, kExt1Id, NULL, CreateHostSuffixFilter("google.com"))));

  listeners_->RemoveLazyListenersForExtension(kExt1Id);

  scoped_ptr<Event> event(CreateNamedEvent(kEvent1Name));
  event->filter_info.SetURL(GURL("http://www.google.com"));
  std::set<const EventListener*> targets(listeners_->GetEventListeners(*event));
  ASSERT_EQ(0u, targets.size());

  event->event_name = kEvent2Name;
  targets = listeners_->GetEventListeners(*event);
  ASSERT_EQ(0u, targets.size());
}

TEST_F(EventListenerMapUnittest, AddExistingFilteredListener) {
  bool first_new = listeners_->AddListener(scoped_ptr<EventListener>(
      new EventListener(kEvent1Name, kExt1Id, NULL,
                        CreateHostSuffixFilter("google.com"))));
  bool second_new = listeners_->AddListener(scoped_ptr<EventListener>(
      new EventListener(kEvent1Name, kExt1Id, NULL,
                        CreateHostSuffixFilter("google.com"))));

  ASSERT_TRUE(first_new);
  ASSERT_FALSE(second_new);
}

TEST_F(EventListenerMapUnittest, AddExistingUnfilteredListener) {
  bool first_add = listeners_->AddListener(scoped_ptr<EventListener>(
      new EventListener(kEvent1Name, kExt1Id, NULL,
                        scoped_ptr<DictionaryValue>())));
  bool second_add = listeners_->AddListener(scoped_ptr<EventListener>(
      new EventListener(kEvent1Name, kExt1Id, NULL,
                        scoped_ptr<DictionaryValue>())));

  scoped_ptr<EventListener> listener(
        new EventListener(kEvent1Name, kExt1Id, NULL,
                          scoped_ptr<DictionaryValue>()));
  bool first_remove = listeners_->RemoveListener(listener.get());
  bool second_remove = listeners_->RemoveListener(listener.get());

  ASSERT_TRUE(first_add);
  ASSERT_FALSE(second_add);
  ASSERT_TRUE(first_remove);
  ASSERT_FALSE(second_remove);
}

TEST_F(EventListenerMapUnittest, RemovingRouters) {
  listeners_->AddListener(scoped_ptr<EventListener>(
      new EventListener(kEvent1Name, kExt1Id, process_.get(),
                        scoped_ptr<DictionaryValue>())));
  listeners_->RemoveListenersForProcess(process_.get());
  ASSERT_FALSE(listeners_->HasListenerForEvent(kEvent1Name));
}

TEST_F(EventListenerMapUnittest, HasListenerForEvent) {
  ASSERT_FALSE(listeners_->HasListenerForEvent(kEvent1Name));

  listeners_->AddListener(scoped_ptr<EventListener>(
      new EventListener(kEvent1Name, kExt1Id, process_.get(),
                        scoped_ptr<DictionaryValue>())));

  ASSERT_FALSE(listeners_->HasListenerForEvent(kEvent2Name));
  ASSERT_TRUE(listeners_->HasListenerForEvent(kEvent1Name));
  listeners_->RemoveListenersForProcess(process_.get());
  ASSERT_FALSE(listeners_->HasListenerForEvent(kEvent1Name));
}

TEST_F(EventListenerMapUnittest, HasListenerForExtension) {
  ASSERT_FALSE(listeners_->HasListenerForExtension(kExt1Id, kEvent1Name));

  // Non-lazy listener.
  listeners_->AddListener(scoped_ptr<EventListener>(
      new EventListener(kEvent1Name, kExt1Id, process_.get(),
                        scoped_ptr<DictionaryValue>())));
  // Lazy listener.
  listeners_->AddListener(scoped_ptr<EventListener>(
      new EventListener(kEvent1Name, kExt1Id, NULL,
                        scoped_ptr<DictionaryValue>())));

  ASSERT_FALSE(listeners_->HasListenerForExtension(kExt1Id, kEvent2Name));
  ASSERT_TRUE(listeners_->HasListenerForExtension(kExt1Id, kEvent1Name));
  ASSERT_FALSE(listeners_->HasListenerForExtension(kExt2Id, kEvent1Name));
  listeners_->RemoveListenersForProcess(process_.get());
  ASSERT_TRUE(listeners_->HasListenerForExtension(kExt1Id, kEvent1Name));
  listeners_->RemoveLazyListenersForExtension(kExt1Id);
  ASSERT_FALSE(listeners_->HasListenerForExtension(kExt1Id, kEvent1Name));
}

TEST_F(EventListenerMapUnittest, AddLazyListenersFromPreferences) {
  scoped_ptr<DictionaryValue> filter1(CreateHostSuffixFilter("google.com"));
  scoped_ptr<DictionaryValue> filter2(CreateHostSuffixFilter("yahoo.com"));

  DictionaryValue filtered_listeners;
  ListValue* filter_list = new ListValue();
  filtered_listeners.Set(kEvent1Name, filter_list);

  filter_list->Append(filter1.release());
  filter_list->Append(filter2.release());

  listeners_->LoadFilteredLazyListeners(kExt1Id, filtered_listeners);

  scoped_ptr<Event> event(CreateEvent(kEvent1Name,
                          GURL("http://www.google.com")));
  std::set<const EventListener*> targets(listeners_->GetEventListeners(*event));
  ASSERT_EQ(1u, targets.size());
  scoped_ptr<EventListener> listener(new EventListener(kEvent1Name, kExt1Id,
      NULL, CreateHostSuffixFilter("google.com")));
  ASSERT_TRUE((*targets.begin())->Equals(listener.get()));
}

TEST_F(EventListenerMapUnittest, CorruptedExtensionPrefsShouldntCrash) {
  scoped_ptr<DictionaryValue> filter1(CreateHostSuffixFilter("google.com"));

  DictionaryValue filtered_listeners;
  // kEvent1Name should be associated with a list, not a dictionary.
  filtered_listeners.Set(kEvent1Name, filter1.release());

  listeners_->LoadFilteredLazyListeners(kExt1Id, filtered_listeners);

  scoped_ptr<Event> event(CreateEvent(kEvent1Name,
                          GURL("http://www.google.com")));
  std::set<const EventListener*> targets(listeners_->GetEventListeners(*event));
  ASSERT_EQ(0u, targets.size());
}

}  // namespace

}  // namespace extensions
