// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/android/sqlite_cursor.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/history/android/android_history_types.h"
#include "content/public/browser/browser_thread.h"
#include "jni/SQLiteCursor_jni.h"
#include "sql/statement.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::GetClass;
using base::android::HasClass;
using base::android::HasMethod;
using base::android::GetMethodID;
using base::android::ScopedJavaLocalRef;
using content::BrowserThread;

namespace {

SQLiteCursor::JavaColumnType ToJavaColumnType(sql::ColType type) {
  switch (type) {
    case sql::COLUMN_TYPE_INTEGER:
      return SQLiteCursor::NUMERIC;
    case sql::COLUMN_TYPE_FLOAT:
      return SQLiteCursor::DOUBLE;
    case sql::COLUMN_TYPE_TEXT:
      return SQLiteCursor::LONG_VAR_CHAR;
    case sql::COLUMN_TYPE_BLOB:
      return SQLiteCursor::BLOB;
    case sql::COLUMN_TYPE_NULL:
      return SQLiteCursor::NULL_TYPE;
    default:
      NOTREACHED();
  }
  return SQLiteCursor::NULL_TYPE;
}

}  // namespace.


SQLiteCursor::TestObserver::TestObserver() {
}

SQLiteCursor::TestObserver::~TestObserver() {
}

ScopedJavaLocalRef<jobject> SQLiteCursor::NewJavaSqliteCursor(
    JNIEnv* env,
    const std::vector<std::string>& column_names,
    history::AndroidStatement* statement,
    AndroidHistoryProviderService* service,
    FaviconService* favicon_service) {
  if (!HasClass(env, kSQLiteCursorClassPath)) {
    LOG(ERROR) << "Can not find " << kSQLiteCursorClassPath;
    return ScopedJavaLocalRef<jobject>();
  }

  ScopedJavaLocalRef<jclass> sclass = GetClass(env, kSQLiteCursorClassPath);
  if (!HasMethod(env, sclass, "<init>", "(I)V")) {
    LOG(ERROR) << "Can not find " << kSQLiteCursorClassPath << " Constructor";
    return ScopedJavaLocalRef<jobject>();
  }

  jmethodID method_id = GetMethodID(env, sclass, "<init>", "(I)V");
  SQLiteCursor* cursor = new SQLiteCursor(column_names, statement, service,
                                          favicon_service);
  ScopedJavaLocalRef<jobject> obj(env,
      env->NewObject(sclass.obj(), method_id, reinterpret_cast<jint>(cursor)));
  if (obj.is_null()) {
    delete cursor;
    return ScopedJavaLocalRef<jobject>();
  }
  return obj;
}

bool SQLiteCursor::RegisterSqliteCursor(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

jint SQLiteCursor::GetCount(JNIEnv* env, jobject obj) {
  // Moves to maxium possible position so we will reach the last row, then finds
  // out the total number of rows.
  int current_position = position_;
  int count = MoveTo(env, obj, std::numeric_limits<int>::max() - 1) + 1;
  // Moves back to the previous position.
  MoveTo(env, obj, current_position);
  return count;
}

ScopedJavaLocalRef<jobjectArray> SQLiteCursor::GetColumnNames(JNIEnv* env,
                                                              jobject obj) {
  size_t count = column_names_.size();
  ScopedJavaLocalRef<jclass> sclass = GetClass(env, "java/lang/String");
  ScopedJavaLocalRef<jobjectArray> arr(env,
      env->NewObjectArray(count, sclass.obj(), NULL));
  for (size_t i = 0; i < count; i++) {
    ScopedJavaLocalRef<jstring> str =
        ConvertUTF8ToJavaString(env, column_names_[i].c_str());
    env->SetObjectArrayElement(arr.obj(), i, str.obj());
  }
  return arr;
}

ScopedJavaLocalRef<jstring> SQLiteCursor::GetString(JNIEnv* env,
                                                    jobject obj,
                                                    jint column) {
  string16 value = statement_->statement()->ColumnString16(column);
  return ScopedJavaLocalRef<jstring>(env,
      env->NewString(value.data(), value.size()));
}

jlong SQLiteCursor::GetLong(JNIEnv* env, jobject obj, jint column) {
  return statement_->statement()->ColumnInt64(column);
}

jint SQLiteCursor::GetInt(JNIEnv* env, jobject obj, jint column) {
  return statement_->statement()->ColumnInt(column);
}

jdouble SQLiteCursor::GetDouble(JNIEnv* env, jobject obj, jint column) {
  return statement_->statement()->ColumnDouble(column);
}

ScopedJavaLocalRef<jbyteArray> SQLiteCursor::GetBlob(JNIEnv* env,
                                                     jobject obj,
                                                     jint column) {
  std::vector<unsigned char> blob;

  // Assume the client will only get favicon using GetBlob.
  if (statement_->favicon_index() == column) {
    if (!GetFavicon(statement_->statement()->ColumnInt(column), &blob))
      return ScopedJavaLocalRef<jbyteArray>();
  } else {
    statement_->statement()->ColumnBlobAsVector(column, &blob);
  }
  ScopedJavaLocalRef<jbyteArray> jb(env, env->NewByteArray(blob.size()));
  int count = 0;
  for (std::vector<unsigned char>::const_iterator i = blob.begin();
      i != blob.end(); ++i) {
    env->SetByteArrayRegion(jb.obj(), count++, 1, (jbyte *)i);
  }
  return jb;
}

jboolean SQLiteCursor::IsNull(JNIEnv* env, jobject obj, jint column) {
  return NULL_TYPE == GetColumnTypeInternal(column) ? JNI_TRUE : JNI_FALSE;
}

jint SQLiteCursor::MoveTo(JNIEnv* env, jobject obj, jint pos) {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&SQLiteCursor::RunMoveStatementOnUIThread,
      base::Unretained(this), pos));
  if (test_observer_)
    test_observer_->OnPostMoveToTask();

  event_.Wait();
  return position_;
}

jint SQLiteCursor::GetColumnType(JNIEnv* env, jobject obj, jint column) {
  return GetColumnTypeInternal(column);
}

void SQLiteCursor::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

SQLiteCursor::SQLiteCursor(const std::vector<std::string>& column_names,
                           history::AndroidStatement* statement,
                           AndroidHistoryProviderService* service,
                           FaviconService* favicon_service)
    : position_(-1),
      event_(false, false),
      statement_(statement),
      column_names_(column_names),
      service_(service),
      favicon_service_(favicon_service),
      count_(-1),
      test_observer_(NULL) {
}

SQLiteCursor::~SQLiteCursor() {
  // Consumer requests were set in the UI thread. They must be cancelled
  // using the same thread.
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    CancelAllRequests(NULL);
  } else {
    base::WaitableEvent event(false, false);
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&SQLiteCursor::CancelAllRequests, base::Unretained(this),
                   &event));
    event.Wait();
  }

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&AndroidHistoryProviderService::CloseStatement,
                 base::Unretained(service_), statement_));
}

bool SQLiteCursor::GetFavicon(history::FaviconID id,
                              std::vector<unsigned char>* image_data) {
  if (id) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&SQLiteCursor::GetFaviconForIDInUIThread,
                   base::Unretained(this), id, &consumer_,
                   base::Bind(&SQLiteCursor::OnFaviconData,
                              base::Unretained(this))));

    if (test_observer_)
      test_observer_->OnPostGetFaviconTask();

    event_.Wait();
    if (!favicon_bitmap_result_.is_valid())
      return false;

    scoped_refptr<base::RefCountedMemory> bitmap_data =
        favicon_bitmap_result_.bitmap_data;
    image_data->assign(bitmap_data->front(),
                       bitmap_data->front() + bitmap_data->size());
    return true;
  }

  return false;
}

void SQLiteCursor::GetFaviconForIDInUIThread(
    history::FaviconID id,
    CancelableRequestConsumerBase* consumer,
    const FaviconService::FaviconRawCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  favicon_service_->GetLargestRawFaviconForID(id, consumer, callback);
}


void SQLiteCursor::OnFaviconData(
    FaviconService::Handle handle,
    const history::FaviconBitmapResult& bitmap_result) {
  favicon_bitmap_result_ = bitmap_result;
  event_.Signal();
  if (test_observer_)
    test_observer_->OnGetFaviconResult();
}

void SQLiteCursor::OnMoved(AndroidHistoryProviderService::Handle handle,
                           int pos) {
  position_ = pos;
  event_.Signal();
  if (test_observer_)
    // Notified test_observer on UI thread instead of the one it will wait.
    test_observer_->OnGetMoveToResult();
}

void SQLiteCursor::CancelAllRequests(base::WaitableEvent* finished) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  consumer_.CancelAllRequests();
  if (finished)
    finished->Signal();
}

SQLiteCursor::JavaColumnType SQLiteCursor::GetColumnTypeInternal(int column) {
  if (column == statement_->favicon_index())
    return SQLiteCursor::BLOB;

  return ToJavaColumnType(statement_->statement()->ColumnType(column));
}

void SQLiteCursor::RunMoveStatementOnUIThread(int pos) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  service_->MoveStatement(statement_, position_, pos, &consumer_,
                base::Bind(&SQLiteCursor::OnMoved, base::Unretained(this)));
}
