// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/stl_util-inl.h"
#include "base/values.h"
#include "crypto/sha2.h"
#include "net/base/crl_filter.h"

#if defined(USE_SYSTEM_ZLIB)
#include <zlib.h>
#else
#include "third_party/zlib/zlib.h"
#endif

namespace net {

// Decompress zlib decompressed |in| into |out|. |out_len| is the number of
// bytes at |out| and must be exactly equal to the size of the decompressed
// data. |dict| optionally contains a pre-shared dictionary.
static bool DecompressZlib(char* out, int out_len, base::StringPiece in,
                           base::StringPiece dict) {
  z_stream z;
  memset(&z, 0, sizeof(z));

  z.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(in.data()));
  z.avail_in = in.size();
  z.next_out = reinterpret_cast<Bytef*>(out);
  z.avail_out = out_len;

  if (inflateInit(&z) != Z_OK)
    return false;
  bool ret = false;
  int r = inflate(&z, Z_FINISH);
  if (r == Z_NEED_DICT) {
    r = inflateSetDictionary(&z, reinterpret_cast<const Bytef*>(dict.data()),
                             dict.size());
    if (r != Z_OK)
      goto err;
    r = inflate(&z, Z_FINISH);
  }
  if (r != Z_STREAM_END)
    goto err;
  if (z.avail_in || z.avail_out)
    goto err;
  ret = true;

 err:
  inflateEnd(&z);
  return ret;
}

/* A RangeDecoder is a type of entropy coder. It is superior to a Huffman
 * encoder because symbols can use fractions of bits.
 *
 * Conceptually a number range is split into regions with one region for each
 * symbol. The size of the region is proportional to the probability of the
 * symbol:
 *
 * +-----+  <- 2**32 - 1
 * |     |
 * |  B  |
 * |     |
 * +-----+  <- 2**30
 * |  A  |
 * +-----+  <- 0
 *
 * Here, symbol B is 3 times more probable than A.
 *
 * This pattern is recursive: it repeats inside each region:
 *
 * +-----+    /+-----+
 * |     |   / |     |
 * |  B  |  /  |  B  |
 * |     | /   |     |
 * +-----+/    +-----+
 * |  A  |     |  A  |
 * +-----+-----+-----+
 *
 * In this implementation, the probabilities are fixed and so are the same at
 * every level.
 *
 * A range coder encodes a series of symbols by specifing a fraction along the
 * number space such that it hits the correct symbols in order. You have to
 * know how many symbols to expect from a range coder because it obviously
 * produces an infinite series of symbols from any input value.
 *
 * In order to make the implementation fast on a computer, a high and low point
 * are maintained that cover the current valid span of the number space.
 * Whenever the span is small enough to that the most significant 8 bits of the
 * high and low values are equal, a byte is produced and the current span is
 * expanded by a factor of 256.
 *
 * A decoder reads these bytes and decodes symbols as required. For example,
 * say that it reads the first byte as 0x80. It knows that the maximum value of
 * the final span is 0x80fffffff... and the minimum value is 0x8000000...
 * That's sufficient to figure out that the first symbol is a B.
 *
 * In the following, we keep track of these values:
 *   high_, low_: the high and low values of the current span. This is needed
 *       to mirror the state of the encoder so that span expansions occur at
 *       the same point.
 *
 *   vhigh_, vlow_: the high and low values of the possible final span.
 *   vbits_: the number of bits of |vhigh_| and |vlow_| that are from data.
 *       (The rest of those values is filled with 0xff or 0x00, respectively.)
 */
class RangeDecoder {
 public:
  // in: the input bytes
  // spans: the probabilities of the symbols. The sum of these values must
  //     equal 2**32 - 1.
  RangeDecoder(base::StringPiece in, const std::vector<uint32>& spans)
      : in_(in),
        spans_(spans),
        high_(-1),
        vhigh_(-1),
        low_(0),
        vlow_(0),
        vbits_(0) {
  }

  bool Decode(unsigned* out_symbol) {
    // high_ and low_ mirror the state of the encoder so, when they agree on
    // the first byte, we have to perform span expansion.
    while ((high_ >> 24) == (low_ >> 24)) {
      vhigh_ <<= 8;
      vhigh_ |= 0xff;
      vlow_ <<= 8;
      vbits_ -= 8;

      high_ <<= 8;
      high_ |= 0xff;
      low_ <<= 8;
    }

    // r is the range of the current span, used as a scaling factor.
    uint64 r = high_ - low_;

    // We consider each symbol in turn and decide if the final span is such
    // that it must be the next symbol.
    for (size_t i = 0; i < spans_.size(); i++) {
      const uint32 span = spans_[i];
      const uint32 scaled = (r * span) >> 32;

      // Since our knowledge of the final span is incremental, |vhigh_| and
      // |vlow_| might be sufficiently far apart that we can't determine the
      // next symbol. In this case we have to read more data.
      while (vhigh_ > (low_ + scaled) && vlow_ <= (low_ + scaled)) {
        // We need more information to disambiguate this. Note that 32-bits of
        // information is always sufficient to disambiguate.
        uint32 b = 0;
        if (!in_.empty())
          b = static_cast<uint8>(in_[0]);
        in_.remove_prefix(1);
        vhigh_ &= ~(static_cast<uint32>(0xff) << (24 - vbits_));
        vhigh_ |= b << (24 - vbits_);
        vlow_ |= b << (24 - vbits_);
        vbits_ += 8;
      }

      // This symbol covers all the possible values for the final span, so this
      // must be the next symbol.
      if (vhigh_ <= (low_ + scaled)) {
        high_ = low_ + scaled;
        *out_symbol = i;
        return true;
      }

      low_ += scaled + 1;
    }

    // Since the sum of |spans_| equals 2**32-1, one of the symbols must cover
    // the current span.
    NOTREACHED();
    return false;
  }

 private:
  base::StringPiece in_;
  const std::vector<uint32> spans_;

  uint32 high_, vhigh_, low_, vlow_;
  unsigned vbits_;

  DISALLOW_COPY_AND_ASSIGN(RangeDecoder);
};

// A GolombCompressedSet is built from a set of random hash values where each
// value is less than a pre-agreed limit. Since the hash values are uniform,
// their differences are geometrically distributed and golomb encoding is the
// optimal encoding for geometrically distributed values.
//
// Thus the set [1, 10, 15] is turned into delta values ([1, 9, 5]) and each
// delta value is Golomb encoded to make a GCS.
//
// Golomb encoding of a value, v, requires knowledge of the geometric
// parameter, M, and consists of (q, r) where v = qM + r. q is unary encoded
// and r is binary encoded. In this code M is fixed at 1024.
//
// A couple of implementation tricks are used to speed things up:
//
// First, the bits are consumed in blocks of 32 and are little endian encoded,
// thus saving a endianness conversion on most systems. Also, the bits inside
// each word are ordered such that the first bit is the least-significant bit
// and the unary encoding is terminated with a 1 rather than the usual 0.
// This allows us to use a DeBruijn sequence to do unary decoding.
class GolombCompressedSet {
 public:
  class iterator {
   public:
    iterator(base::StringPiece data, unsigned num_values)
        : full_data_(data),
          num_values_(num_values) {
      Reset();
    }

    void Reset() {
      data_ = full_data_;
      pending_ = 0;
      bits_pending_ = 0;
      current_ = 0;
    }

    bool Next(uint64* out) {
      unsigned q, r;
      if (!ReadUnary(&q))
        return false;
      if (!ReadBinary10(&r))
        return false;

      uint64 step = static_cast<uint64>(q) << 10;
      step |= r;
      current_ += step;
      *out = current_;
      return true;
    }

    bool NextDelta(unsigned* out_delta) {
      unsigned q, r;
      if (!ReadUnary(&q))
        return false;
      if (!ReadBinary10(&r))
        return false;

      *out_delta = static_cast<unsigned>(q) << 10;
      *out_delta |= r;
      return true;
    }

    bool Contains(uint64 v) {
      Reset();

      uint64 value;
      for (unsigned i = 0; i < num_values_; i++) {
        if (!Next(&value))
          return false;
        if (value == v)
          return true;
        if (value > v)
          return false;
      }

      return false;
    }

   private:
    bool ReadUnary(unsigned* out) {
      *out = 0;

      uint32 w;
      if (!CurrentWord(&w))
        return false;

      while (w == 0) {
        *out += 32;
        if (!CurrentWord(&w))
          return false;
      }

      // A DeBruijn sequence contains all possible subsequences. kDeBruijn is an
      // example of a 32-bit word that contains all possible 5-bit subsequences.
      // When decoding Golomb values, we quickly need to find the number of
      // consequtive zero bits. (w&-w) results in a word with only the
      // least-significant true bit set. Since this work has only a single bit
      // set, its value is a power of two and multiplying by it is the same as a
      // left shift by the position of that bit.
      //
      // Thus we multiply (i.e. left-shift) by the DeBruijn value and check the
      // top 5 bits. Since each 5-bit subsequence in kDeBruijn is unique, we can
      // determine by how many bits it has been shifted with a lookup table.
      static const uint32 kDeBruijn = 0x077CB531;
      static const uint8 kDeBruijnLookup[32] = {
        0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
        31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9,
      };

      MSVC_SUPPRESS_WARNING(4146);
      uint8 r = kDeBruijnLookup[((w & -w) * kDeBruijn) >> 27];
      *out += r;
      pending_ >>= r + 1;
      bits_pending_ -= r + 1;
      return true;
    }

    bool ReadBinary10(unsigned* out) {
      uint32 w;
      if (!CurrentWord(&w))
        return false;
      *out = w & 0x3ff;
      pending_ >>= 10;
      bits_pending_ -= 10;
      return true;
    }

    bool CurrentWord(uint32* out) {
      if (bits_pending_ < 32) {
        if (!ReadWord() && bits_pending_ == 0)
          return false;
      }
      *out = static_cast<uint32>(pending_);
      return true;
    }

    bool ReadWord() {
      DCHECK_LE(bits_pending_, 32u);

      if (data_.size() < 4)
        return false;
      uint32 w;
      memcpy(&w, data_.data(), 4);
      data_.remove_prefix(4);

      uint64 w64 = w;
      w64 <<= bits_pending_;
      pending_ |= w64;
      bits_pending_ += 32;
      return true;
    }

    base::StringPiece full_data_;
    base::StringPiece data_;
    const unsigned num_values_;
    uint64 pending_;
    unsigned bits_pending_;
    uint32 current_;
  };

  GolombCompressedSet(base::StringPiece data,
                      unsigned num_values)
      : full_data_(data),
        num_values_(num_values) {
  }

  iterator begin() const {
    return iterator(full_data_, num_values_);
  }

 private:
  base::StringPiece full_data_;
  const unsigned num_values_;

  DISALLOW_COPY_AND_ASSIGN(GolombCompressedSet);
};

// BitWriter buffers a number of bits in a format that matches
// GolombCompressedSet's expectations: the bits are packed least-significant
// first in little-endian, 32-bit words.
class BitWriter {
 public:
  BitWriter()
      : buf_(NULL),
        buf_len_(0),
        buf_used_(0),
        current_(0),
        num_bits_(0) {
  }

  ~BitWriter() {
    free(buf_);
    buf_ = NULL;
  }

  void WriteBit(bool b) {
    current_ >>= 1;
    if (b)
      current_ |= 0x80000000u;
    num_bits_++;

    if (num_bits_ == sizeof(current_) * 8)
      Flush();
  }

  // WriteGolomb10 outputs v using Golomb encoding with a geometric parameter
  // of 1024.
  void WriteGolomb10(unsigned v) {
    const unsigned q = v >> 10;
    unsigned r = v & 0x3ff;

    for (unsigned i = 0; i < q; i++)
      WriteBit(false);
    WriteBit(true);
    for (unsigned i = 0; i < 10; i++) {
      WriteBit((r & 1) == 1);
      r >>= 1;
    }
  }

  void Flush() {
    if (num_bits_ > 0)
      current_ >>= 32 - num_bits_;

    if (buf_len_ < buf_used_ + sizeof(current_)) {
      if (buf_) {
        buf_len_ += sizeof(current_);
        buf_len_ *= 2;
        buf_ = reinterpret_cast<uint8*>(realloc(buf_, buf_len_));
      } else {
        buf_len_ = 1024;
        buf_ = reinterpret_cast<uint8*>(malloc(buf_len_));
      }
    }
    // assumes little endian
    memcpy(buf_ + buf_used_, &current_, sizeof(current_));
    buf_used_ += sizeof(current_);

    current_ = 0;
    num_bits_ = 0;
  }

  std::string as_string() {
    Flush();
    return std::string(reinterpret_cast<char*>(buf_), buf_used_);
  }

 private:
  uint8* buf_;
  size_t buf_len_;
  size_t buf_used_;
  uint32 current_;
  unsigned num_bits_;

  DISALLOW_COPY_AND_ASSIGN(BitWriter);
};

CRLFilter::CRLFilter()
    : not_before_(0),
      not_after_(0),
      max_range_(0),
      sequence_(0),
      num_entries_(0) {
}

CRLFilter::~CRLFilter() {
}

// CRL filter format:
//
// uint16le description_len
// byte[description_len] description_bytes
// byte[] compressed_header
// byte[] gcs_bytes
//
// description_bytes consists of a JSON dictionary with the following keys:
//   Version (int): currently 0
//   Contents (string): "CRLFilter" or "CRLFilterDelta" (magic value)
//   DeltaFrom (int); if this is a delta filter (see below), then this contains
//       the sequence number of the reference filter.
//   HeaderZLength (int): the number of bytes of compressed header.
//   HeaderLength (int): the number of bytes of header after decompression.
//   RangeLength (int): if this is a delta filter then this is the number of
//       bytes of range coded data.
//
// The uncompressed header is also a JSON dictionary with the following keys:
//   Sequence (int): the sequence number of this filter.
//   Version (int): currently 0.
//   NotBefore (int, epoch seconds): the filter is not valid before this time.
//   NotAfter (int, epoch seconds): the filter is not valid after this time.
//   MaxRange (int): the limit of the GCS encoded values
//   NumEntries (int): the number of GCS entries
//
//   CRLsIncluded (array): the covered CRLs. Each element in the array is a
//       dictionary with the following keys:
//
//       URL (string): the URL of the CRL
//       ParentSPKISHA256 (string): base64 encoded, SHA256 hash of the CRL
//           signer's SPKI.
//
// A delta CRL filter is similar to a CRL filter:
//
// uint16le description_len
// byte[description_len] description_bytes
// byte[] delta_compressed_header
// uint32le[3] range_probabilities
// byte[] range_bytes
// byte[] gcs_bytes
//
// A delta CRL filter applies to a specific CRL filter as given in the
// description's "DeltaFrom" value. The compressed header is compressed with
// the header bytes of the base CRL filter given as a zlib preshared
// dictionary.
//
// range_probabilities contains the probabilies of the three encoded symbols.
// The sum of these values must be 0xffffffff. Next are the range encoded
// bytes, the length of which is given in "RangeLength". There's one symbol for
// each GCS value in the final filter. (This number is given in the
// "NumEntries" value of the header.). Each symbol is either SAME (0), INSERT
// (1) or DELETE (2). SAME values are copied into the new filter, INSERTed
// values are given as a delta from the last value, GCS encoded in |gcs_bytes|.
// DELETEed values are omitted from the final filter.

// ReadDescription reads the description (including length prefix) from |data|
// and updates |data| to remove the description on return. Caller takes
// ownership of the returned pointer.
static DictionaryValue* ReadDescription(base::StringPiece* data) {
  if (data->size() < 2)
    return NULL;
  uint16 description_len;
  memcpy(&description_len, data->data(), 2);  // assumes little-endian.
  data->remove_prefix(2);

  if (data->size() < description_len)
    return NULL;

  const base::StringPiece description_bytes(data->data(), description_len);
  data->remove_prefix(description_len);

  scoped_ptr<Value> description(base::JSONReader::Read(
      description_bytes.as_string(), true /* allow trailing comma */));
  if (description.get() == NULL)
    return NULL;

  if (!description->IsType(Value::TYPE_DICTIONARY))
    return NULL;
  return reinterpret_cast<DictionaryValue*>(description.release());
}

// CRLFilterFromHeader constructs a CRLFilter from the bytes of a header
// structures. The header is JSON. See above for details of the keys.
//
// static
CRLFilter* CRLFilter::CRLFilterFromHeader(base::StringPiece header_bytes) {
  scoped_ptr<Value> header(base::JSONReader::Read(
      header_bytes.as_string(),
      true /* allow trailing comma */));
  if (header.get() == NULL)
    return NULL;

  if (!header->IsType(Value::TYPE_DICTIONARY))
    return NULL;
  DictionaryValue* header_dict =
      reinterpret_cast<DictionaryValue*>(header.get());
  int version;
  if (!header_dict->GetInteger("Version", &version) ||
      version != 0) {
    return NULL;
  }

  double not_before, not_after, max_range, num_entries;
  if (!header_dict->GetDouble("NotBefore", &not_before) ||
      !header_dict->GetDouble("NotAfter", &not_after) ||
      !header_dict->GetDouble("NumEntries", &num_entries) ||
      !header_dict->GetDouble("MaxRange", &max_range)) {
    return NULL;
  }

  if (not_before <= 0 || not_after <= 0 || max_range <= 0 || num_entries <= 0)
    return NULL;

  int sequence;
  if (!header_dict->GetInteger("Sequence", &sequence) ||
      sequence <= 0) {
    // Sequence is assumed to be zero if omitted.
    sequence = 0;
  }

  scoped_ptr<CRLFilter> crl_filter(new CRLFilter);
  crl_filter->sequence_ = sequence;
  crl_filter->not_before_ = not_before;
  crl_filter->not_after_ = not_after;
  crl_filter->max_range_ = max_range;
  crl_filter->num_entries_ = num_entries;
  crl_filter->header_bytes_ = header_bytes.as_string();

  ListValue* crls_included;
  if (!header_dict->GetList("CRLsIncluded", &crls_included))
    return NULL;

  for (size_t i = 0; i < crls_included->GetSize(); i++) {
    DictionaryValue* included_crl_dict;
    if (!crls_included->GetDictionary(i, &included_crl_dict))
      return NULL;
    std::string url, parent_spki_sha256_b64;
    if (!included_crl_dict->GetString("URL", &url) ||
        !included_crl_dict->GetString("ParentSPKISHA256",
                                      &parent_spki_sha256_b64)) {
      return NULL;
    }

    std::string parent_spki_sha256;
    if (!base::Base64Decode(parent_spki_sha256_b64,
                            &parent_spki_sha256)) {
      return NULL;
    }
    crl_filter->crls_included_.insert(
        std::make_pair<std::string, std::string>(
            url,
            parent_spki_sha256));
  }

  return crl_filter.release();
}

// kMaxHeaderLengthBytes contains the sanity limit of the size of a CRL
// filter's decompressed header.
static const int kMaxHeaderLengthBytes = 1024 * 1024;

// static
bool CRLFilter::Parse(base::StringPiece data,
                      scoped_refptr<CRLFilter>* out_crl_filter) {
  // Other parts of Chrome assume that we're little endian, so we don't lose
  // anything by doing this.
#if defined(__BYTE_ORDER)
  // Linux check
  COMPILE_ASSERT(__BYTE_ORDER == __LITTLE_ENDIAN,
                 datapack_assumes_little_endian);
#elif defined(__BIG_ENDIAN__)
  // Mac check
  #error DataPack assumes little endian
#endif

  scoped_ptr<DictionaryValue> description_dict(
      ReadDescription(&data));
  if (!description_dict.get())
    return false;

  std::string contents;
  if (!description_dict->GetString("Contents", &contents))
    return false;
  if (contents != "CRLFilter")
    return false;

  int version;
  if (!description_dict->GetInteger("Version", &version) ||
      version != 0) {
    return false;
  }

  int compressed_header_len;
  if (!description_dict->GetInteger("HeaderZLength", &compressed_header_len))
    return false;

  if (compressed_header_len <= 0 ||
      data.size() < static_cast<unsigned>(compressed_header_len)) {
    return false;
  }
  const base::StringPiece compressed_header(data.data(), compressed_header_len);
  data.remove_prefix(compressed_header_len);

  int header_len;
  if (!description_dict->GetInteger("HeaderLength", &header_len))
    return false;
  if (header_len < 0 || header_len > kMaxHeaderLengthBytes) {
    NOTREACHED();
    return false;
  }

  scoped_array<char> header_bytes(new char[header_len]);
  base::StringPiece no_dict;
  if (!DecompressZlib(header_bytes.get(), header_len, compressed_header,
                      no_dict)) {
    return false;
  }

  scoped_refptr<CRLFilter> crl_filter(CRLFilterFromHeader(
        base::StringPiece(header_bytes.get(), header_len)));

  if (!crl_filter.get())
    return false;

  // The remainder is the Golomb Compressed Set.
  crl_filter->gcs_bytes_ = data.as_string();
  crl_filter->gcs_.reset(new GolombCompressedSet(crl_filter->gcs_bytes_,
                                                 crl_filter->num_entries_));
  *out_crl_filter = crl_filter;
  return true;
}

bool CRLFilter::ApplyDelta(base::StringPiece data,
                           scoped_refptr<CRLFilter>* out_crl_filter) {
  scoped_ptr<DictionaryValue> description_dict(
      ReadDescription(&data));
  if (!description_dict.get())
    return false;

  int compressed_header_len, header_len, delta_from, version, range_length;
  std::string contents;
  if (!description_dict->GetInteger("HeaderZLength", &compressed_header_len) ||
      !description_dict->GetInteger("HeaderLength", &header_len) ||
      !description_dict->GetInteger("RangeLength", &range_length) ||
      !description_dict->GetInteger("DeltaFrom", &delta_from) ||
      !description_dict->GetInteger("Version", &version) ||
      !description_dict->GetString("Contents", &contents)) {
    return false;
  }

  if (version != 0 || contents != "CRLFilterDelta")
    return false;

  if (delta_from < 0 || static_cast<unsigned>(delta_from) != sequence_)
    return false;

  if (compressed_header_len <= 0 ||
      data.size() < static_cast<unsigned>(compressed_header_len) ||
      header_len < 0 ||
      header_len > kMaxHeaderLengthBytes) {
    return false;
  }

  const base::StringPiece compressed_header(data.data(), compressed_header_len);
  data.remove_prefix(compressed_header_len);

  scoped_array<char> header_bytes(new char[header_len]);
  if (!DecompressZlib(header_bytes.get(), header_len, compressed_header,
                      header_bytes_)) {
    return false;
  }

  scoped_refptr<CRLFilter> crl_filter(CRLFilterFromHeader(
        base::StringPiece(header_bytes.get(), header_len)));

  if (!crl_filter.get())
    return false;

  // Next are the three span values.
  static const unsigned kNumSpanValues = 3;
  static const size_t kNumSpanBytes = kNumSpanValues * sizeof(uint32);
  if (data.size() < kNumSpanBytes)
    return false;

  std::vector<uint32> spans(kNumSpanBytes);
  memcpy(&spans[0], data.data(), kNumSpanBytes);
  data.remove_prefix(kNumSpanBytes);

  if (data.size() < static_cast<unsigned>(range_length))
    return false;
  RangeDecoder decoder(data.substr(0, range_length), spans);
  data.remove_prefix(range_length);

  GolombCompressedSet gcs(data, 0 /* no values; we don't know that yet. */);
  GolombCompressedSet::iterator gcs_deltas(gcs.begin());
  GolombCompressedSet::iterator gcs_prev(gcs_->begin());
  BitWriter bitwriter;

  uint64 last = 0, v;
  for (unsigned i = 0; i < crl_filter->num_entries_;) {
    unsigned symbol, delta;
    if (!decoder.Decode(&symbol))
      return false;
    if (symbol == SYMBOL_SAME) {
      if (!gcs_prev.Next(&v))
        return false;
      bitwriter.WriteGolomb10(v - last);
      last = v;
      i++;
    } else if (symbol == SYMBOL_INSERT) {
      if (!gcs_deltas.NextDelta(&delta))
        return false;
      bitwriter.WriteGolomb10(delta);
      last += delta;
      i++;
    } else if (symbol == SYMBOL_DELETE) {
      if (!gcs_prev.Next(&v))
        return false;
    } else {
      NOTREACHED();
      return false;
    }
  }

  crl_filter->gcs_bytes_ = bitwriter.as_string();
  crl_filter->gcs_.reset(new GolombCompressedSet(crl_filter->gcs_bytes_,
                                                 crl_filter->num_entries_));
  *out_crl_filter = crl_filter;
  return true;
}

bool CRLFilter::CRLIsCovered(
    const std::vector<base::StringPiece>& crl_urls,
    const std::string& parent_spki_sha256) {
  for (std::vector<base::StringPiece>::const_iterator
       i = crl_urls.begin(); i != crl_urls.end(); i++) {
    if (ContainsKey(crls_included_, std::make_pair<std::string, std::string>(
            i->as_string(), parent_spki_sha256))) {
      return true;
    }
  }
  return false;
}

// FNV1a64 computes the FNV1a 64-bit hash of the concatenation of |a| and
// |b|.
static uint64 FNV1a64(const std::string& a, const std::string& b) {
  uint64 x = 14695981039346656037ull;
  static const uint64 p = 1099511628211ull;
  for (size_t i = 0; i < a.size(); i++) {
    x ^= static_cast<uint8>(a[i]);
    x *= p;
  }
  for (size_t i = 0; i < b.size(); i++) {
    x ^= static_cast<uint8>(b[i]);
    x *= p;
  }
  return x;
}

CRLFilter::Result CRLFilter::CheckCertificate(
    base::StringPiece cert_spki,
    const std::string& serial_number,
    const std::vector<base::StringPiece>& crl_urls,
    base::StringPiece parent_spki) {
  const std::string parent_spki_sha256 =
      crypto::SHA256HashString(parent_spki.as_string());

  if (!CRLIsCovered(crl_urls, parent_spki_sha256))
    return UNKNOWN;

  uint64 h = FNV1a64(serial_number, parent_spki_sha256);
  h %= max_range_;

  GolombCompressedSet::iterator it(gcs_->begin());
  if (it.Contains(h))
    return PROBABLY_REVOKED;
  return NOT_REVOKED;
}

int64 CRLFilter::not_before() const {
  return not_before_;
}

int64 CRLFilter::not_after() const {
  return not_after_;
}

uint64 CRLFilter::max_range() const {
  return max_range_;
}

unsigned CRLFilter::num_entries() const {
  return num_entries_;
}

std::vector<uint64> CRLFilter::DebugValues() {
  std::vector<uint64> ret;
  uint64 v;

  GolombCompressedSet::iterator it(gcs_->begin());

  for (unsigned i = 0; i < num_entries_; i++) {
    if (!it.Next(&v)) {
      ret.clear();
      break;
    }
    ret.push_back(v);
  }
  return ret;
}

std::string CRLFilter::SHA256() const {
  std::string s = header_bytes_;
  s += gcs_bytes_;
  return crypto::SHA256HashString(s);
}

}  // namespace net
