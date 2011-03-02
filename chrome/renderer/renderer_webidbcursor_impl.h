// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_RENDERER_WEBIDBCURSOR_IMPL_H_
#define CHROME_RENDERER_RENDERER_WEBIDBCURSOR_IMPL_H_

#include "base/basictypes.h"
#include "chrome/common/indexed_db_key.h"
#include "chrome/common/serialized_script_value.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBCallbacks.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBCursor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBKey.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSerializedScriptValue.h"

class RendererWebIDBCursorImpl : public WebKit::WebIDBCursor {
 public:
  RendererWebIDBCursorImpl(int32 idb_cursor_id);
  virtual ~RendererWebIDBCursorImpl();

  virtual unsigned short direction() const;
  virtual WebKit::WebIDBKey key() const;
  virtual WebKit::WebIDBKey primaryKey() const;
  virtual void value(WebKit::WebSerializedScriptValue&,
                     WebKit::WebIDBKey&) const;
  virtual void update(const WebKit::WebSerializedScriptValue& value,
                      WebKit::WebIDBCallbacks* callback,
                      WebKit::WebExceptionCode& ec);
  virtual void continueFunction(const WebKit::WebIDBKey& key,
                                WebKit::WebIDBCallbacks* callback,
                                WebKit::WebExceptionCode& ec);
  virtual void remove(WebKit::WebIDBCallbacks* callback,
                      WebKit::WebExceptionCode& ec);

 private:
  int32 idb_cursor_id_;
};

#endif  // CHROME_RENDERER_RENDERER_WEBIDBCURSOR_IMPL_H_
