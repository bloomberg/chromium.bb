// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_MESSAGE_FILTER_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_MESSAGE_FILTER_H_

#include "base/compiler_specific.h"
#include "content/public/browser/browser_message_filter.h"
#include "url/gurl.h"

class Profile;
struct PrerenderAttributes;

namespace content {
struct Referrer;
}

namespace gfx {
class Size;
}

namespace IPC {
class Message;
}

namespace prerender {

class PrerenderMessageFilter : public content::BrowserMessageFilter {
 public:
  PrerenderMessageFilter(int render_process_id, Profile* profile);

 private:
  virtual ~PrerenderMessageFilter();

  // Overridden from content::BrowserMessageFilter.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OverrideThreadForMessage(
      const IPC::Message& message,
      content::BrowserThread::ID* thread) OVERRIDE;
  virtual void OnChannelClosing() OVERRIDE;

  void OnAddPrerender(int prerender_id,
                      const PrerenderAttributes& attributes,
                      const content::Referrer& referrer,
                      const gfx::Size& size,
                      int render_view_route_id);
  void OnCancelPrerender(int prerender_id);
  void OnAbandonPrerender(int prerender_id);

  const int render_process_id_;
  Profile* const profile_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderMessageFilter);
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_MESSAGE_FILTER_H_

