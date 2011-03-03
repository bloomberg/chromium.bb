// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/metrics_helpers.h"

#if defined(USE_SYSTEM_LIBBZ2)
#include <bzlib.h>
#else
#include "third_party/bzip2/bzlib.h"
#endif

#include "base/base64.h"
#include "base/time.h"
#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/md5.h"
#include "base/perftimer.h"
#include "base/string_number_conversions.h"
#include "base/sys_info.h"
#include "base/utf_string_conversions.h"
#include "base/third_party/nspr/prtime.h"
#include "chrome/common/logging_chrome.h"
#include "googleurl/src/gurl.h"
#include "libxml/xmlwriter.h"

#define OPEN_ELEMENT_FOR_SCOPE(name) ScopedElement scoped_element(this, name)

using base::Histogram;
using base::StatisticsRecorder;
using base::Time;
using base::TimeDelta;

// http://blogs.msdn.com/oldnewthing/archive/2004/10/25/247180.aspx
#if defined(OS_WIN)
extern "C" IMAGE_DOS_HEADER __ImageBase;
#endif

namespace {

// libxml take xmlChar*, which is unsigned char*
inline const unsigned char* UnsignedChar(const char* input) {
  return reinterpret_cast<const unsigned char*>(input);
}

}  // namespace

class MetricsLogBase::XmlWrapper {
 public:
  XmlWrapper()
      : doc_(NULL),
        buffer_(NULL),
        writer_(NULL) {
    buffer_ = xmlBufferCreate();
    CHECK(buffer_);

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

// static
std::string MetricsLogBase::version_extension_;

MetricsLogBase::MetricsLogBase(const std::string& client_id, int session_id,
                               const std::string& version_string)
    : start_time_(Time::Now()),
      client_id_(client_id),
      session_id_(base::IntToString(session_id)),
      locked_(false),
      xml_wrapper_(new XmlWrapper),
      num_events_(0) {

  StartElement("log");
  WriteAttribute("clientid", client_id_);
  WriteInt64Attribute("buildtime", GetBuildTime());
  WriteAttribute("appversion", version_string);
}

MetricsLogBase::~MetricsLogBase() {
  delete xml_wrapper_;
}

void MetricsLogBase::CloseLog() {
  DCHECK(!locked_);
  locked_ = true;

  int result = xmlTextWriterEndDocument(xml_wrapper_->writer());
  DCHECK_GE(result, 0);

  result = xmlTextWriterFlush(xml_wrapper_->writer());
  DCHECK_GE(result, 0);

#if defined(OS_CHROMEOS)
  xmlNodePtr root = xmlDocGetRootElement(xml_wrapper_->doc());
  if (!hardware_class_.empty()) {
    // The hardware class is determined after the first ongoing log is
    // constructed, so this adds the root element's "hardwareclass"
    // attribute when the log is closed instead.
    xmlNewProp(root, UnsignedChar("hardwareclass"),
               UnsignedChar(hardware_class_.c_str()));
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

int MetricsLogBase::GetEncodedLogSize() {
  DCHECK(locked_);
  CHECK(xml_wrapper_);
  CHECK(xml_wrapper_->buffer());
  return xml_wrapper_->buffer()->use;
}

bool MetricsLogBase::GetEncodedLog(char* buffer, int buffer_size) {
  DCHECK(locked_);
  if (buffer_size < GetEncodedLogSize())
    return false;

  memcpy(buffer, xml_wrapper_->buffer()->content, GetEncodedLogSize());
  return true;
}

std::string MetricsLogBase::GetEncodedLogString() {
  DCHECK(locked_);
  return std::string(reinterpret_cast<char*>(xml_wrapper_->buffer()->content));
}

int MetricsLogBase::GetElapsedSeconds() {
  return static_cast<int>((Time::Now() - start_time_).InSeconds());
}

std::string MetricsLogBase::CreateHash(const std::string& value) {
  MD5Context ctx;
  MD5Init(&ctx);
  MD5Update(&ctx, value.data(), value.length());

  MD5Digest digest;
  MD5Final(&digest, &ctx);

  uint64 reverse_uint64;
  // UMA only uses first 8 chars of hash. We use the above uint64 instead
  // of a unsigned char[8] so that we don't run into strict aliasing issues
  // in the LOG statement below when trying to interpret reverse as a uint64.
  unsigned char* reverse = reinterpret_cast<unsigned char *>(&reverse_uint64);
  DCHECK(arraysize(digest.a) >= sizeof(reverse_uint64));
  for (size_t i = 0; i < sizeof(reverse_uint64); ++i)
    reverse[i] = digest.a[sizeof(reverse_uint64) - i - 1];
  // The following log is VERY helpful when folks add some named histogram into
  // the code, but forgot to update the descriptive list of histograms.  When
  // that happens, all we get to see (server side) is a hash of the histogram
  // name.  We can then use this logging to find out what histogram name was
  // being hashed to a given MD5 value by just running the version of Chromium
  // in question with --enable-logging.
  VLOG(1) << "Metrics: Hash numeric [" << value
          << "]=[" << reverse_uint64 << "]";
  return std::string(reinterpret_cast<char*>(digest.a), arraysize(digest.a));
}

std::string MetricsLogBase::CreateBase64Hash(const std::string& string) {
  std::string encoded_digest;
  if (base::Base64Encode(CreateHash(string), &encoded_digest)) {
    DVLOG(1) << "Metrics: Hash [" << encoded_digest << "]=[" << string << "]";
    return encoded_digest;
  }
  return std::string();
}

void MetricsLogBase::RecordUserAction(const char* key) {
  DCHECK(!locked_);

  std::string command_hash = CreateBase64Hash(key);
  if (command_hash.empty()) {
    NOTREACHED() << "Unable generate encoded hash of command: " << key;
    return;
  }

  OPEN_ELEMENT_FOR_SCOPE("uielement");
  WriteAttribute("action", "command");
  WriteAttribute("targetidhash", command_hash);

  // TODO(jhughes): Properly track windows.
  WriteIntAttribute("window", 0);
  WriteCommonEventAttributes();

  ++num_events_;
}

void MetricsLogBase::RecordLoadEvent(int window_id,
                                 const GURL& url,
                                 PageTransition::Type origin,
                                 int session_index,
                                 TimeDelta load_time) {
  DCHECK(!locked_);

  OPEN_ELEMENT_FOR_SCOPE("document");
  WriteAttribute("action", "load");
  WriteIntAttribute("docid", session_index);
  WriteIntAttribute("window", window_id);
  WriteAttribute("loadtime", base::Int64ToString(load_time.InMilliseconds()));

  std::string origin_string;

  switch (PageTransition::StripQualifier(origin)) {
    // TODO(jhughes): Some of these mappings aren't right... we need to add
    // some values to the server's enum.
    case PageTransition::LINK:
    case PageTransition::MANUAL_SUBFRAME:
      origin_string = "link";
      break;

    case PageTransition::TYPED:
      origin_string = "typed";
      break;

    case PageTransition::AUTO_BOOKMARK:
      origin_string = "bookmark";
      break;

    case PageTransition::AUTO_SUBFRAME:
    case PageTransition::RELOAD:
      origin_string = "refresh";
      break;

    case PageTransition::GENERATED:
    case PageTransition::KEYWORD:
      origin_string = "global-history";
      break;

    case PageTransition::START_PAGE:
      origin_string = "start-page";
      break;

    case PageTransition::FORM_SUBMIT:
      origin_string = "form-submit";
      break;

    default:
      NOTREACHED() << "Received an unknown page transition type: " <<
                       PageTransition::StripQualifier(origin);
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

// static
int64 MetricsLogBase::GetBuildTime() {
  static int64 integral_build_time = 0;
  if (!integral_build_time) {
    Time time;
    const char* kDateTime = __DATE__ " " __TIME__ " GMT";
    bool result = Time::FromString(ASCIIToWide(kDateTime).c_str(), &time);
    DCHECK(result);
    integral_build_time = static_cast<int64>(time.ToTimeT());
  }
  return integral_build_time;
}

MetricsLog* MetricsLogBase::AsMetricsLog() {
  return NULL;
}

// TODO(JAR): A The following should really be part of the histogram class.
// Internal state is being needlessly exposed, and it would be hard to reuse
// this code. If we moved this into the Histogram class, then we could use
// the same infrastructure for logging StatsCounters, RatesCounters, etc.
void MetricsLogBase::RecordHistogramDelta(
    const Histogram& histogram,
    const Histogram::SampleSet& snapshot) {
  DCHECK(!locked_);
  DCHECK_NE(0, snapshot.TotalCount());
  snapshot.CheckSize(histogram);

  // We will ignore the MAX_INT/infinite value in the last element of range[].

  OPEN_ELEMENT_FOR_SCOPE("histogram");

  WriteAttribute("name", CreateBase64Hash(histogram.histogram_name()));

  WriteInt64Attribute("sum", snapshot.sum());
  WriteInt64Attribute("sumsquares", snapshot.square_sum());

  for (size_t i = 0; i < histogram.bucket_count(); i++) {
    if (snapshot.counts(i)) {
      OPEN_ELEMENT_FOR_SCOPE("histogrambucket");
      WriteIntAttribute("min", histogram.ranges(i));
      WriteIntAttribute("max", histogram.ranges(i + 1));
      WriteIntAttribute("count", snapshot.counts(i));
    }
  }
}


// MetricsServiceBase
MetricsServiceBase::MetricsServiceBase()
    : pending_log_(NULL),
      compressed_log_(),
      current_log_(NULL) {
}

MetricsServiceBase::~MetricsServiceBase() {
  if (pending_log_) {
    delete pending_log_;
    pending_log_ = NULL;
  }
  if (current_log_) {
    delete current_log_;
    current_log_ = NULL;
  }
}

// This implementation is based on the Firefox MetricsService implementation.
bool MetricsServiceBase::Bzip2Compress(const std::string& input,
                                       std::string* output) {
  bz_stream stream = {0};
  // As long as our input is smaller than the bzip2 block size, we should get
  // the best compression.  For example, if your input was 250k, using a block
  // size of 300k or 500k should result in the same compression ratio.  Since
  // our data should be under 100k, using the minimum block size of 100k should
  // allocate less temporary memory, but result in the same compression ratio.
  int result = BZ2_bzCompressInit(&stream,
                                  1,   // 100k (min) block size
                                  0,   // quiet
                                  0);  // default "work factor"
  if (result != BZ_OK) {  // out of memory?
    return false;
  }

  output->clear();

  stream.next_in = const_cast<char*>(input.data());
  stream.avail_in = static_cast<int>(input.size());
  // NOTE: we don't need a BZ_RUN phase since our input buffer contains
  //       the entire input
  do {
    output->resize(output->size() + 1024);
    stream.next_out = &((*output)[stream.total_out_lo32]);
    stream.avail_out = static_cast<int>(output->size()) - stream.total_out_lo32;
    result = BZ2_bzCompress(&stream, BZ_FINISH);
  } while (result == BZ_FINISH_OK);
  if (result != BZ_STREAM_END)  // unknown failure?
    return false;
  result = BZ2_bzCompressEnd(&stream);
  DCHECK(result == BZ_OK);

  output->resize(stream.total_out_lo32);

  return true;
}

void MetricsServiceBase::DiscardPendingLog() {
  if (pending_log_) {  // Shutdown might have deleted it!
    delete pending_log_;
    pending_log_ = NULL;
  }
  compressed_log_.clear();
}

void MetricsServiceBase::RecordCurrentHistograms() {
  DCHECK(current_log_);
  TransmitAllHistograms(base::Histogram::kNoFlags, true);
}

void MetricsServiceBase::TransmitHistogramDelta(
      const base::Histogram& histogram,
      const base::Histogram::SampleSet& snapshot) {
  current_log_->RecordHistogramDelta(histogram, snapshot);
}

void MetricsServiceBase::InconsistencyDetected(int problem) {
  UMA_HISTOGRAM_ENUMERATION("Histogram.InconsistenciesBrowser",
                            problem, Histogram::NEVER_EXCEEDED_VALUE);
}

void MetricsServiceBase::UniqueInconsistencyDetected(int problem) {
  UMA_HISTOGRAM_ENUMERATION("Histogram.InconsistenciesBrowserUnique",
                            problem, Histogram::NEVER_EXCEEDED_VALUE);
}

void MetricsServiceBase::SnapshotProblemResolved(int amount) {
  UMA_HISTOGRAM_COUNTS("Histogram.InconsistentSnapshotBrowser",
                       std::abs(amount));
}

HistogramSender::HistogramSender() {}

HistogramSender::~HistogramSender() {}

void HistogramSender::TransmitAllHistograms(Histogram::Flags flag_to_set,
                                            bool send_only_uma) {
  StatisticsRecorder::Histograms histograms;
  StatisticsRecorder::GetHistograms(&histograms);
  for (StatisticsRecorder::Histograms::const_iterator it = histograms.begin();
       histograms.end() != it;
       ++it) {
    (*it)->SetFlags(flag_to_set);
    if (send_only_uma &&
        0 == ((*it)->flags() & Histogram::kUmaTargetedHistogramFlag))
      continue;
    TransmitHistogram(**it);
  }
}

void HistogramSender::TransmitHistogram(const Histogram& histogram) {
  // Get up-to-date snapshot of sample stats.
  Histogram::SampleSet snapshot;
  histogram.SnapshotSample(&snapshot);
  const std::string& histogram_name = histogram.histogram_name();

  int corruption = histogram.FindCorruption(snapshot);
  if (corruption) {
    NOTREACHED();
    InconsistencyDetected(corruption);
    // Don't send corrupt data to metrics survices.
    if (NULL == inconsistencies_.get())
      inconsistencies_.reset(new ProblemMap);
    int old_corruption = (*inconsistencies_)[histogram_name];
    if (old_corruption == (corruption | old_corruption))
      return;  // We've already seen this corruption for this histogram.
    (*inconsistencies_)[histogram_name] |= corruption;
    UniqueInconsistencyDetected(corruption);
    return;
  }

  // Find the already sent stats, or create an empty set.  Remove from our
  // snapshot anything that we've already sent.
  LoggedSampleMap::iterator it = logged_samples_.find(histogram_name);
  Histogram::SampleSet* already_logged;
  if (logged_samples_.end() == it) {
    // Add new entry
    already_logged = &logged_samples_[histogram.histogram_name()];
    already_logged->Resize(histogram);  // Complete initialization.
  } else {
    already_logged = &(it->second);
    int64 discrepancy(already_logged->TotalCount() -
                    already_logged->redundant_count());
    if (discrepancy) {
      NOTREACHED();  // Already_logged has become corrupt.
      int problem = static_cast<int>(discrepancy);
      if (problem != discrepancy)
        problem = INT_MAX;
      SnapshotProblemResolved(problem);
      // With no valid baseline, we'll act like we've sent everything in our
      // snapshot.
      already_logged->Subtract(*already_logged);
      already_logged->Add(snapshot);
    }
    // Deduct any stats we've already logged from our snapshot.
    snapshot.Subtract(*already_logged);
  }

  // Snapshot now contains only a delta to what we've already_logged.
  if (snapshot.redundant_count() > 0) {
    TransmitHistogramDelta(histogram, snapshot);
    // Add new data into our running total.
    already_logged->Add(snapshot);
  }
}
