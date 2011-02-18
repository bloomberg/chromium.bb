// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "content/browser/tab_contents/provisional_load_details.h"

#include "chrome/browser/ssl/ssl_manager.h"

ProvisionalLoadDetails::ProvisionalLoadDetails(bool is_main_frame,
                                               bool is_in_page_navigation,
                                               const GURL& url,
                                               const std::string& security_info,
                                               bool is_content_filtered,
                                               bool is_error_page,
                                               int64 frame_id)
      : error_code_(net::OK),
        transition_type_(PageTransition::LINK),
        url_(url),
        is_main_frame_(is_main_frame),
        is_in_page_navigation_(is_in_page_navigation),
        ssl_cert_id_(0),
        ssl_cert_status_(0),
        ssl_security_bits_(-1),
        ssl_connection_status_(0),
        is_content_filtered_(is_content_filtered),
        is_error_page_(is_error_page),
        frame_id_(frame_id) {
  SSLManager::DeserializeSecurityInfo(security_info,
                                      &ssl_cert_id_,
                                      &ssl_cert_status_,
                                      &ssl_security_bits_,
                                      &ssl_connection_status_);
}
