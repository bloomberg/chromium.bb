// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/metrics/metrics_log_base.h"

#include "base/base64.h"
#include "base/basictypes.h"
#include "base/md5.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/perftimer.h"
#include "base/string_number_conversions.h"
#include "base/sys_byteorder.h"
#include "base/sys_info.h"
#include "base/third_party/nspr/prtime.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/metrics/proto/histogram_event.pb.h"
#include "chrome/common/metrics/proto/system_profile.pb.h"
#include "chrome/common/metrics/proto/user_action_event.pb.h"
#include "libxml/xmlwriter.h"

#define OPEN_ELEMENT_FOR_SCOPE(name) ScopedElement scoped_element(this, name)

using base::Histogram;
using base::HistogramBase;
using base::HistogramSamples;
using base::SampleCountIterator;
using base::Time;
using base::TimeDelta;
using metrics::HistogramEventProto;
using metrics::SystemProfileProto;
using metrics::UserActionEventProto;

namespace {

// libxml take xmlChar*, which is unsigned char*
inline const unsigned char* UnsignedChar(const char* input) {
  return reinterpret_cast<const unsigned char*>(input);
}

// Any id less than 16 bytes is considered to be a testing id.
bool IsTestingID(const std::string& id) {
  return id.size() < 16;
}

// Converts the 8-byte prefix of an MD5 hash into a uint64 value.
inline uint64 HashToUInt64(const std::string& hash) {
  uint64 value;
  DCHECK_GE(hash.size(), sizeof(value));
  memcpy(&value, hash.data(), sizeof(value));
  return base::HostToNet64(value);
}

// Creates an MD5 hash of the given value, and returns hash as a byte buffer
// encoded as an std::string.
std::string CreateHash(const std::string& value) {
  base::MD5Context context;
  base::MD5Init(&context);
  base::MD5Update(&context, value);

  base::MD5Digest digest;
  base::MD5Final(&digest, &context);

  std::string hash(reinterpret_cast<char*>(digest.a), arraysize(digest.a));

  // The following log is VERY helpful when folks add some named histogram into
  // the code, but forgot to update the descriptive list of histograms.  When
  // that happens, all we get to see (server side) is a hash of the histogram
  // name.  We can then use this logging to find out what histogram name was
  // being hashed to a given MD5 value by just running the version of Chromium
  // in question with --enable-logging.
  DVLOG(1) << "Metrics: Hash numeric [" << value << "]=[" << HashToUInt64(hash)
           << "]";

  return hash;
}

SystemProfileProto::Channel AsProtobufChannel(
    chrome::VersionInfo::Channel channel) {
  switch (channel) {
    case chrome::VersionInfo::CHANNEL_UNKNOWN:
      return SystemProfileProto::CHANNEL_UNKNOWN;
    case chrome::VersionInfo::CHANNEL_CANARY:
      return SystemProfileProto::CHANNEL_CANARY;
    case chrome::VersionInfo::CHANNEL_DEV:
      return SystemProfileProto::CHANNEL_DEV;
    case chrome::VersionInfo::CHANNEL_BETA:
      return SystemProfileProto::CHANNEL_BETA;
    case chrome::VersionInfo::CHANNEL_STABLE:
      return SystemProfileProto::CHANNEL_STABLE;
    default:
      NOTREACHED();
      return SystemProfileProto::CHANNEL_UNKNOWN;
  }
}

}  // namespace

class MetricsLogBase::XmlWrapper {
 public:
  XmlWrapper()
      : doc_(NULL),
        buffer_(NULL),
        writer_(NULL) {
    buffer_ = xmlBufferCreate();
    DCHECK(buffer_);

    #if defined(OS_CHROMEOS)
      writer_ = xmlNewTextWriterDoc(&doc_, /* compression */ 0);
    #else
      writer_ = xmlNewTextWriterMemory(buffer_, /* compression */ 0);
    #endif  // OS_CHROMEOS
    DCHECK(writer_);

    int result = xmlTextWriterSetIndent(writer_, 2);
    DCHECK_EQ(0, result);
  }

  ~XmlWrapper() {
    FreeDocWriter();
    if (buffer_) {
      xmlBufferFree(buffer_);
      buffer_ = NULL;
    }
  }

  void FreeDocWriter() {
    if (writer_) {
      xmlFreeTextWriter(writer_);
      writer_ = NULL;
    }
    if (doc_) {
      xmlFreeDoc(doc_);
      doc_ = NULL;
    }
  }

  xmlDocPtr doc() const { return doc_; }
  xmlTextWriterPtr writer() const { return writer_; }
  xmlBufferPtr buffer() const { return buffer_; }

 private:
  xmlDocPtr doc_;
  xmlBufferPtr buffer_;
  xmlTextWriterPtr writer_;
};

MetricsLogBase::MetricsLogBase(const std::string& client_id, int session_id,
                               const std::string& version_string)
    : num_events_(0),
      start_time_(Time::Now()),
      client_id_(client_id),
      session_id_(base::IntToString(session_id)),
      locked_(false),
      xml_wrapper_(new XmlWrapper) {
  int64_t build_time = GetBuildTime();

  // Write the XML version.
  StartElement("log");
  WriteAttribute("clientid", client_id_);
  WriteInt64Attribute("buildtime", build_time);
  WriteAttribute("appversion", version_string);

  // Write the protobuf version.
  if (IsTestingID(client_id_)) {
    uma_proto_.set_client_id(0);
  } else {
    uma_proto_.set_client_id(HashToUInt64(CreateHash(client_id)));
  }
  uma_proto_.set_session_id(session_id);
  uma_proto_.mutable_system_profile()->set_build_timestamp(build_time);
  uma_proto_.mutable_system_profile()->set_app_version(version_string);
  uma_proto_.mutable_system_profile()->set_channel(
      AsProtobufChannel(chrome::VersionInfo::GetChannel()));
}

MetricsLogBase::~MetricsLogBase() {
  if (!locked_) {
    locked_ = true;
    int result = xmlTextWriterEndDocument(xml_wrapper_->writer());
    DCHECK_GE(result, 0);
  }
  delete xml_wrapper_;
  xml_wrapper_ = NULL;
}

// static
void MetricsLogBase::CreateHashes(const std::string& string,
                                  std::string* base64_encoded_hash,
                                  uint64* numeric_hash) {
  std::string hash = CreateHash(string);

  std::string encoded_digest;
  if (base::Base64Encode(hash, &encoded_digest))
    DVLOG(1) << "Metrics: Hash [" << encoded_digest << "]=[" << string << "]";

  *base64_encoded_hash = encoded_digest;
  *numeric_hash = HashToUInt64(hash);
}

// static
int64 MetricsLogBase::GetBuildTime() {
  static int64 integral_build_time = 0;
  if (!integral_build_time) {
    Time time;
    const char* kDateTime = __DATE__ " " __TIME__ " GMT";
    bool result = Time::FromString(kDateTime, &time);
    DCHECK(result);
    integral_build_time = static_cast<int64>(time.ToTimeT());
  }
  return integral_build_time;
}

// static
int64 MetricsLogBase::GetCurrentTime() {
  return (base::TimeTicks::Now() - base::TimeTicks()).InSeconds();
}

void MetricsLogBase::CloseLog() {
  DCHECK(!locked_);
  locked_ = true;

  int result = xmlTextWriterEndDocument(xml_wrapper_->writer());
  DCHECK_GE(result, 0);

  result = xmlTextWriterFlush(xml_wrapper_->writer());
  DCHECK_GE(result, 0);

#if defined(OS_CHROMEOS)
  // TODO(isherman): Once the XML pipeline is deprecated, there will be no need
  // to track the hardware class in a separate member variable and only write it
  // out when the log is closed.
  xmlNodePtr root = xmlDocGetRootElement(xml_wrapper_->doc());
  if (!hardware_class_.empty()) {
    // The hardware class is determined after the first ongoing log is
    // constructed, so this adds the root element's "hardwareclass"
    // attribute when the log is closed instead.
    xmlNewProp(root, UnsignedChar("hardwareclass"),
               UnsignedChar(hardware_class_.c_str()));

    // Write to the protobuf too.
    uma_proto_.mutable_system_profile()->mutable_hardware()->set_hardware_class(
        hardware_class_);
  }

  // Flattens the XML tree into a character buffer.
  PerfTimer dump_timer;
  result = xmlNodeDump(xml_wrapper_->buffer(), xml_wrapper_->doc(),
                       root, /* level */ 0, /* format */ 1);
  DCHECK_GE(result, 0);
  UMA_HISTOGRAM_TIMES("UMA.XMLNodeDumpTime", dump_timer.Elapsed());

  PerfTimer free_timer;
  xml_wrapper_->FreeDocWriter();
  UMA_HISTOGRAM_TIMES("UMA.XMLWriterDestructionTime", free_timer.Elapsed());
#endif  // OS_CHROMEOS
}

int MetricsLogBase::GetEncodedLogSizeXml() {
  DCHECK(locked_);
  CHECK(xml_wrapper_);
  CHECK(xml_wrapper_->buffer());
  return xml_wrapper_->buffer()->use;
}

bool MetricsLogBase::GetEncodedLogXml(char* buffer, int buffer_size) {
  DCHECK(locked_);
  if (buffer_size < GetEncodedLogSizeXml())
    return false;

  memcpy(buffer, xml_wrapper_->buffer()->content, GetEncodedLogSizeXml());
  return true;
}

void MetricsLogBase::GetEncodedLogProto(std::string* encoded_log) {
  DCHECK(locked_);
  uma_proto_.SerializeToString(encoded_log);
}

int MetricsLogBase::GetElapsedSeconds() {
  return static_cast<int>((Time::Now() - start_time_).InSeconds());
}

void MetricsLogBase::RecordUserAction(const char* key) {
  DCHECK(!locked_);

  std::string base64_hash;
  uint64 numeric_hash;
  CreateHashes(key, &base64_hash, &numeric_hash);
  if (base64_hash.empty()) {
    NOTREACHED() << "Unable generate encoded hash of command: " << key;
    return;
  }

  // Write the XML version.
  OPEN_ELEMENT_FOR_SCOPE("uielement");
  WriteAttribute("action", "command");
  WriteAttribute("targetidhash", base64_hash);

  // Write the protobuf version.
  UserActionEventProto* user_action = uma_proto_.add_user_action_event();
  user_action->set_name_hash(numeric_hash);
  user_action->set_time(MetricsLogBase::GetCurrentTime());

  // TODO(jhughes): Properly track windows.
  WriteIntAttribute("window", 0);
  WriteCommonEventAttributes();

  ++num_events_;
}

void MetricsLogBase::RecordLoadEvent(int window_id,
                                     const GURL& url,
                                     content::PageTransition origin,
                                     int session_index,
                                     TimeDelta load_time) {
  DCHECK(!locked_);

  OPEN_ELEMENT_FOR_SCOPE("document");
  WriteAttribute("action", "load");
  WriteIntAttribute("docid", session_index);
  WriteIntAttribute("window", window_id);
  WriteAttribute("loadtime", base::Int64ToString(load_time.InMilliseconds()));

  std::string origin_string;

  switch (content::PageTransitionStripQualifier(origin)) {
    // TODO(jhughes): Some of these mappings aren't right... we need to add
    // some values to the server's enum.
    case content::PAGE_TRANSITION_LINK:
    case content::PAGE_TRANSITION_MANUAL_SUBFRAME:
      origin_string = "link";
      break;

    case content::PAGE_TRANSITION_TYPED:
      origin_string = "typed";
      break;

    case content::PAGE_TRANSITION_AUTO_BOOKMARK:
      origin_string = "bookmark";
      break;

    case content::PAGE_TRANSITION_AUTO_SUBFRAME:
    case content::PAGE_TRANSITION_RELOAD:
      origin_string = "refresh";
      break;

    case content::PAGE_TRANSITION_GENERATED:
    case content::PAGE_TRANSITION_KEYWORD:
      origin_string = "global-history";
      break;

    case content::PAGE_TRANSITION_AUTO_TOPLEVEL:
      origin_string = "auto-toplevel";
      break;

    case content::PAGE_TRANSITION_FORM_SUBMIT:
      origin_string = "form-submit";
      break;

    default:
      NOTREACHED() << "Received an unknown page transition type: " <<
                      content::PageTransitionStripQualifier(origin);
  }
  if (!origin_string.empty())
    WriteAttribute("origin", origin_string);

  WriteCommonEventAttributes();

  ++num_events_;
}

void MetricsLogBase::RecordWindowEvent(WindowEventType type,
                                   int window_id,
                                   int parent_id) {
  DCHECK(!locked_);

  OPEN_ELEMENT_FOR_SCOPE("window");
  WriteAttribute("action", WindowEventTypeToString(type));
  WriteAttribute("windowid", base::IntToString(window_id));
  if (parent_id >= 0)
    WriteAttribute("parent", base::IntToString(parent_id));
  WriteCommonEventAttributes();

  ++num_events_;
}

std::string MetricsLogBase::GetCurrentTimeString() {
  return base::Uint64ToString(Time::Now().ToTimeT());
}

// These are the attributes that are common to every event.
void MetricsLogBase::WriteCommonEventAttributes() {
  WriteAttribute("session", session_id_);
  WriteAttribute("time", GetCurrentTimeString());
}

void MetricsLogBase::WriteAttribute(const std::string& name,
                                    const std::string& value) {
  DCHECK(!locked_);
  DCHECK(!name.empty());

  int result = xmlTextWriterWriteAttribute(xml_wrapper_->writer(),
                                           UnsignedChar(name.c_str()),
                                           UnsignedChar(value.c_str()));
  DCHECK_GE(result, 0);
}

void MetricsLogBase::WriteIntAttribute(const std::string& name, int value) {
  WriteAttribute(name, base::IntToString(value));
}

void MetricsLogBase::WriteInt64Attribute(const std::string& name, int64 value) {
  WriteAttribute(name, base::Int64ToString(value));
}

// static
const char* MetricsLogBase::WindowEventTypeToString(WindowEventType type) {
  switch (type) {
    case WINDOW_CREATE:  return "create";
    case WINDOW_OPEN:    return "open";
    case WINDOW_CLOSE:   return "close";
    case WINDOW_DESTROY: return "destroy";

    default:
      NOTREACHED();
      return "unknown";  // Can't return NULL as this is used in a required
                         // attribute.
  }
}

void MetricsLogBase::StartElement(const char* name) {
  DCHECK(!locked_);
  DCHECK(name);

  int result = xmlTextWriterStartElement(xml_wrapper_->writer(),
                                         UnsignedChar(name));
  DCHECK_GE(result, 0);
}

void MetricsLogBase::EndElement() {
  DCHECK(!locked_);

  int result = xmlTextWriterEndElement(xml_wrapper_->writer());
  DCHECK_GE(result, 0);
}

// TODO(JAR): A The following should really be part of the histogram class.
// Internal state is being needlessly exposed, and it would be hard to reuse
// this code. If we moved this into the Histogram class, then we could use
// the same infrastructure for logging StatsCounters, RatesCounters, etc.
void MetricsLogBase::RecordHistogramDelta(const std::string& histogram_name,
                                          const HistogramSamples& snapshot) {
  DCHECK(!locked_);
  DCHECK_NE(0, snapshot.TotalCount());

  // We will ignore the MAX_INT/infinite value in the last element of range[].

  OPEN_ELEMENT_FOR_SCOPE("histogram");

  std::string base64_name_hash;
  uint64 numeric_name_hash;
  CreateHashes(histogram_name, &base64_name_hash, &numeric_name_hash);

  // Write the XML version.
  WriteAttribute("name", base64_name_hash);

  WriteInt64Attribute("sum", snapshot.sum());
  // TODO(jar): Remove sumsquares when protobuffer accepts this as optional.
  WriteInt64Attribute("sumsquares", 0);

  for (scoped_ptr<SampleCountIterator> it = snapshot.Iterator();
       !it->Done();
       it->Next()) {
    OPEN_ELEMENT_FOR_SCOPE("histogrambucket");
    HistogramBase::Sample min;
    HistogramBase::Sample max;
    HistogramBase::Count count;
    it->Get(&min, &max, &count);
    WriteIntAttribute("min", min);
    WriteIntAttribute("max", max);
    WriteIntAttribute("count", count);
  }

  // Write the protobuf version.
  HistogramEventProto* histogram_proto = uma_proto_.add_histogram_event();
  histogram_proto->set_name_hash(numeric_name_hash);
  histogram_proto->set_sum(snapshot.sum());

  for (scoped_ptr<SampleCountIterator> it = snapshot.Iterator();
       !it->Done();
       it->Next()) {
    HistogramBase::Sample min;
    HistogramBase::Sample max;
    HistogramBase::Count count;
    it->Get(&min, &max, &count);
    HistogramEventProto::Bucket* bucket = histogram_proto->add_bucket();
    bucket->set_min(min);
    bucket->set_max(max);
    bucket->set_count(count);

    size_t index;
    if (it->GetBucketIndex(&index))
      bucket->set_bucket_index(index);
  }
}
