// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/communicator/mailbox.h"

#include <assert.h>
#include <stdlib.h>

#include <stack>
#include <vector>

#include "chrome/browser/sync/notifier/base/string.h"
#include "chrome/browser/sync/notifier/base/utils.h"
#include "chrome/browser/sync/notifier/communicator/xml_parse_helpers.h"
#include "talk/base/basictypes.h"
#include "talk/base/common.h"
#include "talk/base/stringutils.h"
#include "talk/xmllite/xmlelement.h"
#include "talk/xmpp/constants.h"

namespace notifier {

// Labels are a list of strings seperated by a '|' character. The '|' character
// is escaped with a backslash ('\\') and the backslash is also escaped with a
// backslash.
static void ParseLabelSet(const std::string& text,
                          MessageThread::StringSet* labels) {
  const char* input_cur = text.c_str();
  const char* input_end = input_cur + text.size();
  char* result = new char[text.size() + 1];
  char* next_write = result;

  while(input_cur < input_end) {
    if (*input_cur == '|') {
      if (next_write != result) {
        *next_write = '\0';
        labels->insert(std::string(result));
        next_write = result;
      }
      input_cur++;
      continue;
    }

    if (*input_cur == '\\') {
      // Skip a character in the input and break if we are at the end.
      input_cur++;
      if (input_cur >= input_end)
        break;
    }
    *next_write = *input_cur;
    next_write++;
    input_cur++;
  }

  if (next_write != result) {
    *next_write = '\0';
    labels->insert(std::string(result));
  }

  delete [] result;
}

// -----------------------------------------------------------------------------

std::string MailAddress::safe_name() const {
  if (!name().empty()) {
    return name();
  }

  if (!address().empty()) {
    size_t at = address().find('@');
    if (at == std::string::npos) {
      return address();
    }

    if (at != 0) {
      return address().substr(0, at);
    }
  }

  return std::string("(unknown)");
}

// -----------------------------------------------------------------------------
MessageThread::~MessageThread() {
  Clear();
}

void MessageThread::Clear() {
  delete labels_;
  labels_ = NULL;

  delete senders_;
  senders_ = NULL;
}

MessageThread& MessageThread::operator=(const MessageThread& r) {
  if (&r != this) {
    Clear();
    // Copy everything.
    r.AssertValid();
    thread_id_ = r.thread_id_;
    date64_ = r.date64_;
    message_count_ = r.message_count_;
    personal_level_ = r.personal_level_;
    subject_ = r.subject_;
    snippet_ = r.snippet_;

    if (r.labels_)
      labels_ = new StringSet(*r.labels_);
    else
      labels_ = new StringSet;
    if (r.senders_)
      senders_ = new MailSenderList(*r.senders_);
    else
      senders_ = new MailSenderList;
  }
  AssertValid();
  return *this;
}

MessageThread* MessageThread::CreateFromXML(
    const buzz::XmlElement* src) {
  MessageThread* info = new MessageThread();
  if (!info || !info->InitFromXml(src)) {
    delete info;
    return NULL;
  }
  return info;
}

// Init from a chunk of XML.
bool MessageThread::InitFromXml(const buzz::XmlElement* src) {
  labels_ = new StringSet;
  senders_ = new MailSenderList;

  if (src->Name() != buzz::kQnMailThreadInfo)
    return false;

  if (!ParseInt64Attr(src, buzz::kQnMailTid, &thread_id_))
    return false;
  if (!ParseInt64Attr(src, buzz::kQnMailDate, &date64_))
    return false;
  if (!ParseIntAttr(src, buzz::kQnMailMessages, &message_count_))
    return false;
  if (!ParseIntAttr(src, buzz::kQnMailParticipation, &personal_level_))
    return false;

  const buzz::XmlElement* senders = src->FirstNamed(buzz::kQnMailSenders);
  if (!senders)
    return false;
  for (const buzz::XmlElement* child = senders->FirstElement();
       child != NULL;
       child = child->NextElement()) {
    if (child->Name() != buzz::kQnMailSender)
      continue;
    std::string address;
    if (!ParseStringAttr(child, buzz::kQnMailAddress, &address))
      continue;
    std::string name;
    ParseStringAttr(child, buzz::kQnMailName, &name);
    bool originator = false;
    ParseBoolAttr(child, buzz::kQnMailOriginator, &originator);
    bool unread = false;
    ParseBoolAttr(child, buzz::kQnMailUnread, &unread);

    senders_->push_back(MailSender(name, address, unread, originator));
  }

  const buzz::XmlElement* labels = src->FirstNamed(buzz::kQnMailLabels);
  if (!labels)
    return false;
  ParseLabelSet(labels->BodyText(), labels_);

  const buzz::XmlElement* subject = src->FirstNamed(buzz::kQnMailSubject);
  if (!subject)
    return false;
  subject_ = subject->BodyText();

  const buzz::XmlElement* snippet = src->FirstNamed(buzz::kQnMailSnippet);
  if (!snippet)
    return false;
  snippet_ = snippet->BodyText();

  AssertValid();
  return true;
}

bool MessageThread::starred() const {
  return (labels_->find("^t") != labels_->end());
}

bool MessageThread::unread() const {
  return (labels_->find("^u") != labels_->end());
}

#if defined(DEBUG)
// Non-debug version is inline and empty.
void MessageThread::AssertValid() const {
  assert(thread_id_ != 0);
  assert(senders_ != NULL);
  // In some (odd) cases, gmail may send email with no sender.
  // assert(!senders_->empty());
  assert(message_count_ > 0);
  assert(labels_ != NULL);
}
#endif

MailBox* MailBox::CreateFromXML(const buzz::XmlElement* src) {
  MailBox* mail_box = new MailBox();
  if (!mail_box || !mail_box->InitFromXml(src)) {
    delete mail_box;
    return NULL;
  }
  return mail_box;
}

// Init from a chunk of XML.
bool MailBox::InitFromXml(const buzz::XmlElement* src) {
  if (src->Name() != buzz::kQnMailBox)
    return false;

  if (!ParseIntAttr(src, buzz::kQnMailTotalMatched, &mailbox_size_))
    return false;

  estimate_ = false;
  ParseBoolAttr(src, buzz::kQnMailTotalEstimate, &estimate_);

  first_index_ = 0;
  ParseIntAttr(src, buzz::kQnMailFirstIndex, &first_index_);

  result_time_ = 0;
  ParseInt64Attr(src, buzz::kQnMailResultTime, &result_time_);

  highest_thread_id_ = 0;

  const buzz::XmlElement* thread_element = src->FirstNamed(buzz::kQnMailThreadInfo);
  while (thread_element) {
    MessageThread* thread = MessageThread::CreateFromXML(thread_element);
    if (thread) {
      if (thread->thread_id() > highest_thread_id_)
        highest_thread_id_ = thread->thread_id();
      threads_.push_back(MessageThreadPointer(thread));
    }
    thread_element = thread_element->NextNamed(buzz::kQnMailThreadInfo);
  }
  return true;
}

const size_t kMaxShortnameLength = 12;

// Tip: If you extend this list of chars, do not include '-'.
const char name_delim[] = " ,.:;\'\"()[]{}<>*@";

class SenderFormatter {
 public:
  // sender should not be deleted while this class is in use.
  SenderFormatter(const MailSender& sender,
                  const std::string& me_address)
      : sender_(sender),
        visible_(false),
        short_format_(true),
        space_(kMaxShortnameLength) {
    me_ = talk_base::ascicmp(me_address.c_str(),
                             sender.address().c_str()) == 0;
  }

  bool visible() const {
    return visible_;
  }

  bool is_unread() const {
    return sender_.unread();
  }

  const std::string name() const {
    return name_;
  }

  void set_short_format(bool short_format) {
    short_format_ = short_format;
    UpdateName();
  }

  void set_visible(bool visible) {
    visible_ = visible;
    UpdateName();
  }

  void set_space(size_t space) {
    space_ = space;
    UpdateName();
  }

 private:
  // Attempt to shorten to the first word in a person's name We could revisit
  // and do better at international punctuation, but this is what cricket did,
  // and it should be removed soon when gmail does the notification instead of
  // us forming it on the client.
  static void ShortenName(std::string* name) {
    size_t start = name->find_first_not_of(name_delim);
    if (start != std::string::npos && start > 0) {
      name->erase(0, start);
    }
    start = name->find_first_of(name_delim);
    if (start != std::string::npos) {
      name->erase(start);
    }
  }

  void UpdateName() {
    // Update the name if is going to be used.
    if (!visible_) {
      return;
    }

    if (me_) {
      name_ = "me";
      return;
    }

    if (sender_.name().empty() && sender_.address().empty()) {
      name_ = "";
      return;
    }

    name_ = sender_.name();
    // Handle the case of no name or a name looks like an email address. When
    // mail is sent to "Quality@example.com" <quality-team@example.com>, we
    // shouldn't show "Quality@example.com" as the name. Instead, use the email
    // address (without the @...)
    if (name_.empty() || name_.find_first_of("@") != std::string::npos) {
      name_ = sender_.address();
      size_t at_index = name_.find_first_of("@");
      if (at_index != std::string::npos) {
        name_.erase(at_index);
      }
    } else if (short_format_) {
      ShortenName(&name_);
    }

    if (name_.empty()) {
      name_ = "(unknown)";
    }

    // Abbreviate if too long.
    if (name_.size() > space_) {
      name_.replace(space_ - 1, std::string::npos, ".");
    }
  }

  const MailSender& sender_;
  std::string name_;
  bool visible_;
  bool short_format_;
  size_t space_;
  bool me_;
  DISALLOW_COPY_AND_ASSIGN(SenderFormatter);
};

const char kNormalSeparator[] = ",&nbsp;";
const char kEllidedSeparator[] = "&nbsp;..";

std::string FormatName(const std::string& name, bool bold) {
  std::string formatted_name;
  if (bold) {
    formatted_name.append("<b>");
  }
  formatted_name.append(HtmlEncode(name));
  if (bold) {
    formatted_name.append("</b>");
  }
  return formatted_name;
}

class SenderFormatterList {
 public:
  // sender_list must not be deleted while this class is being used.
  SenderFormatterList(const MailSenderList& sender_list,
                      const std::string& me_address)
      : state_(INITIAL_STATE),
        are_any_read_(false),
        index_(-1),
        first_unread_index_(-1) {
    // Add all read messages.
    const MailSender* originator = NULL;
    bool any_unread = false;
    for (size_t i = 0; i < sender_list.size(); ++i) {
      if (sender_list[i].originator()) {
        originator = &sender_list[i];
      }
      if (sender_list[i].unread()) {
        any_unread = true;
        continue;
      }
      are_any_read_ = true;
      if (!sender_list[i].originator()) {
        senders_.push_back(new SenderFormatter(sender_list[i],
                                               me_address));
      }
    }

    // There may not be an orignator (if there no senders).
    if (originator) {
      senders_.insert(senders_.begin(), new SenderFormatter(*originator,
                                                            me_address));
    }

    // Add all unread messages.
    if (any_unread) {
      // It should be rare, but there may be cases when all of the senders
      // appear to have read the message.
      first_unread_index_ = is_first_unread() ? 0 : senders_.size();
      for (size_t i = 0; i < sender_list.size(); ++i) {
        if (!sender_list[i].unread()) {
          continue;
        }
        // Don't add the originator if it is already at the start of the
        // "unread" list.
        if (sender_list[i].originator() && is_first_unread()) {
          continue;
        }
        senders_.push_back(new SenderFormatter(sender_list[i], me_address));
      }
    }
  }

  ~SenderFormatterList() {
    CleanupSequence(&senders_);
  }

  std::string GetHtml(int space) {
    index_ = -1;
    state_ = INITIAL_STATE;
    while (!added_.empty()) {
      added_.pop();
    }

    // If there is only one sender, let it take up all of the space.
    if (senders_.size() == 1) {
      senders_[0]->set_space(space);
      senders_[0]->set_short_format(false);
    }

    int length = 1;
    // Add as many senders as we can in the given space. Computes the visible
    // length at each iteration, but does not construct the actual html.
    while (length < space && AddNextSender()) {
      int new_length = ConstructHtml(is_first_unread(), NULL);
      // Remove names to avoid truncating
      // * if there will be at least 2 left or
      // * if the spacing <= 2 characters per sender and there
      //   is at least one left.
      if ((new_length > space &&
           (visible_count() > 2 ||
            (ComputeSpacePerSender(space) <= 2 && visible_count() > 1)))) {
        RemoveLastAddedSender();
        break;
      }
      length = new_length;
    }

    if (length > space) {
      int max = ComputeSpacePerSender(space);
      for (size_t i = 0; i < senders_.size(); ++i) {
        if (senders_[i]->visible()) {
          senders_[i]->set_space(max);
        }
      }
    }

    // Now construct the actual html.
    std::string html_list;
    length = ConstructHtml(is_first_unread(), &html_list);
    if (length > space) {
      LOG(LS_WARNING) << "LENGTH: " << length << " exceeds "
                      << space << " " << html_list;
    }
    return html_list;
  }

 private:
  int ComputeSpacePerSender(int space) const {
    // Why the "- 2"? To allow for the " .. " which may occur after the names,
    // and no matter what always allow at least 2 characters per sender.
    return talk_base::_max<int>(space / visible_count() - 2, 2);
  }

  // Finds the next sender that should be added to the "from" list and sets it
  // to visible.
  //
  // This method may be called until it returns false or until
  // RemoveLastAddedSender is called.
  bool AddNextSender() {
    // The progression is:
    //   1. Add the person who started the thread, which is the first message.
    //   2. Add the first unread message (unless it has already been added).
    //   3. Add the last message (unless it has already been added).
    //   4. Add the message that is just before the last message processed
    //      (unless it has already been added).
    //      If there is no message (i.e. at index -1), return false.
    //
    // Typically, this method is called until it returns false or all of the
    // space available is used.
    switch (state_) {
      case INITIAL_STATE:
        state_ = FIRST_MESSAGE;
        index_ = 0;
        // If the server behaves odd and doesn't send us any senders, do
        // something graceful.
        if (senders_.size() == 0) {
          return false;
        }
        break;

      case FIRST_MESSAGE:
        if (first_unread_index_ >= 0) {
          state_ = FIRST_UNREAD_MESSAGE;
          index_ = first_unread_index_;
          break;
        }
        // Fall through.
      case FIRST_UNREAD_MESSAGE:
        state_ = LAST_MESSAGE;
        index_ = senders_.size() - 1;
        break;

      case LAST_MESSAGE:
      case PREVIOUS_MESSAGE:
        state_ = PREVIOUS_MESSAGE;
        index_--;
        break;

      case REMOVED_MESSAGE:
      default:
        ASSERT(false);
        return false;
    }

    if (index_ < 0) {
      return false;
    }

    if (!senders_[index_]->visible()) {
      added_.push(index_);
      senders_[index_]->set_visible(true);
    }
    return true;
  }

  // Makes the last added sender not visible.
  //
  // May be called while visible_count() > 0.
  void RemoveLastAddedSender() {
    state_ = REMOVED_MESSAGE;
    int index = added_.top();
    added_.pop();
    senders_[index]->set_visible(false);
  }

  // Constructs the html of the SenderList and returns the length of the
  // visible text.
  //
  // The algorithm simply walks down the list of Senders, appending the html
  // for each visible sender, and adding ellipsis or commas in between,
  // whichever is appropriate.
  //
  // html Filled with html.  Maybe NULL if the html doesn't need to be
  //      constructed yet (useful for simply determining the length of the
  //      visible text).
  //
  // returns The approximate visible length of the html.
  int ConstructHtml(bool first_is_unread,
                    std::string* html) const {
    if (senders_.empty()) {
      return 0;
    }

    int length = 0;

    // The first is always visible.
    const SenderFormatter* sender = senders_[0];
    const std::string& originator_name = sender->name();
    length += originator_name.length();
    if (html) {
      html->append(FormatName(originator_name, first_is_unread));
    }

    bool elided = false;
    const char* between = "";
    for (size_t i = 1; i < senders_.size(); i++) {
      sender = senders_[i];

      if (sender->visible()) {
        // Handle the separator.
        between = elided ? "&nbsp;" : kNormalSeparator;
        // Ignore the , for length because it is so narrow, so in both cases
        // above the space is the only things that counts for spaces.
        length++;

        // Handle the name.
        const std::string name = sender->name();
        length += name.size();

        // Construct the html.
        if (html) {
          html->append(between);
          html->append(FormatName(name, sender->is_unread()));
        }
        elided = false;
      } else if (!elided) {
        between = kEllidedSeparator;
        length += 2;  // ".." is narrow.
        if (html) {
          html->append(between);
        }
        elided = true;
      }
    }
    return length;
  }

  bool is_first_unread() const {
    return !are_any_read_;
  }

  size_t visible_count() const {
    return added_.size();
  }

  enum MessageState {
    INITIAL_STATE,
    FIRST_MESSAGE,
    FIRST_UNREAD_MESSAGE,
    LAST_MESSAGE,
    PREVIOUS_MESSAGE,
    REMOVED_MESSAGE,
  };

  // What state we were in during the last "stateful" function call.
  MessageState state_;
  bool are_any_read_;
  std::vector<SenderFormatter*> senders_;
  std::stack<int> added_;
  int index_;
  int first_unread_index_;
  DISALLOW_COPY_AND_ASSIGN(SenderFormatterList);
};


std::string GetSenderHtml(const MailSenderList& sender_list,
                          int message_count,
                          const std::string& me_address,
                          int space) {
  // There has to be at least 9 spaces to show something reasonable.
  ASSERT(space >= 10);
  std::string count_html;
  if (message_count > 1) {
    std::string count(IntToString(message_count));
    space -= (count.size());
    count_html.append("&nbsp;(");
    count_html.append(count);
    count_html.append(")");
    // Reduce space due to parenthesis and &nbsp;.
    space -= 2;
  }

  SenderFormatterList senders(sender_list, me_address);
  std::string html_list(senders.GetHtml(space));
  html_list.append(count_html);
  return html_list;
}

}  // namespace notifier
