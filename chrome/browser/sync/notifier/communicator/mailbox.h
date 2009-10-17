// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_COMMUNICATOR_MAILBOX_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_COMMUNICATOR_MAILBOX_H_

#include <set>
#include <string>
#include <vector>

#include "talk/base/basictypes.h"
#include "talk/base/linked_ptr.h"

namespace buzz {
class XmlElement;
}

namespace notifier {
// -----------------------------------------------------------------------------
class MailAddress {
 public:
  MailAddress(const std::string& name, const std::string& address)
    : name_(name),
      address_(address) {
  }
  const std::string& name() const    { return name_; }
  const std::string& address() const { return address_; }
  std::string safe_name() const; // Will return *something*.
 private:
  std::string name_;
  std::string address_;
};

// -----------------------------------------------------------------------------
class MailSender : public MailAddress {
 public:
  MailSender(const std::string& name, const std::string& address, bool unread,
             bool originator)
    : MailAddress(name, address),
    unread_(unread),
    originator_(originator) {
  }

  MailSender(const MailSender& r)
    : MailAddress(r.name(), r.address()) {
    unread_ = r.unread_;
    originator_ = r.originator_;
  }

  bool unread() const     { return unread_; }
  bool originator() const { return originator_; }

 private:
  bool unread_;
  bool originator_;
};

typedef std::vector<MailSender> MailSenderList;

// -----------------------------------------------------------------------------
// MessageThread: everything there is to know about a mail thread.
class MessageThread {
 public:
  MessageThread(const MessageThread& r) {
    labels_ = NULL;
    senders_ = NULL;
    *this = r;
  }

  ~MessageThread();

  // Try to parse the XML to create a MessageThreadInfo.  If NULL is returned
  // then we either ran out of memory or there was an error in parsing the XML.
  static MessageThread* CreateFromXML(const buzz::XmlElement* src);

  MessageThread& operator=(const MessageThread& r);

  // SameThreadAs : name is self evident.
  bool SameThreadAs(const MessageThread& r) {
    AssertValid();
    r.AssertValid();
    return (thread_id_ == r.thread_id_);
  }

  // SameAs : true if thread has same id and messages.
  // Assumes that messages don't disappear from threads.
  bool SameAs(const MessageThread& r) {
    AssertValid();
    r.AssertValid();
    return SameThreadAs(r) &&
           message_count_ == r.message_count_;
  }

  typedef std::set<std::string> StringSet;

  int64 thread_id() const         { return thread_id_; }
  const StringSet* labels() const { return labels_; }
  int64 date64() const            { return date64_; }
  MailSenderList* senders() const { return senders_; }
  int personal_level() const      { return personal_level_; }
  int message_count() const       { return message_count_; }
  const std::string& subject() const  { return subject_; }
  const std::string& snippet() const  { return snippet_; }
  bool starred() const;
  bool unread() const;

#if defined(DEBUG)
  void AssertValid() const;
#else
  inline void AssertValid() const {}
#endif

 private:
  void Clear();

 private:
  MessageThread() : senders_(NULL), labels_(NULL) {}
  bool InitFromXml(const buzz::XmlElement* src);

  int64 thread_id_;
  int64 date64_;
  int message_count_;
  int personal_level_;
  std::string subject_;
  std::string snippet_;
  MailSenderList* senders_;
  StringSet* labels_;
};

typedef talk_base::linked_ptr<MessageThread> MessageThreadPointer;
typedef std::vector<MessageThreadPointer> MessageThreadVector;

// -----------------------------------------------------------------------------
class MailBox {
 public:
  static MailBox* CreateFromXML(const buzz::XmlElement* src);

  const MessageThreadVector& threads() const { return threads_; }
  int mailbox_size() const { return mailbox_size_; }
  int first_index() const { return first_index_; }
  bool estimate() const { return estimate_; }
  int64 result_time() const { return result_time_; }
  int64 highest_thread_id() const { return highest_thread_id_; }

 private:
  MailBox() {}
  bool InitFromXml(const buzz::XmlElement* src);

  MessageThreadVector threads_;

  int mailbox_size_;
  int first_index_;
  bool estimate_;
  int64 result_time_;
  int64 highest_thread_id_;
};

std::string GetSenderHtml(const MailSenderList& sender_list,
                          int message_count,
                          const std::string& me_address,
                          int space);

}  // namespace notifier

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_COMMUNICATOR_MAILBOX_H_
