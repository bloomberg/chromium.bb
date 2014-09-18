// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILE_RESETTER_JTL_FOUNDATION_H_
#define CHROME_BROWSER_PROFILE_RESETTER_JTL_FOUNDATION_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "crypto/hmac.h"

namespace jtl_foundation {

// A JTL (JSON Traversal Language) program is composed of one or more
// sentences. Each sentence consists of a sequence of operations. The input of
// the program is a hierarchical JSON data structure.
//
// The execution of each sentence starts at the root of an input dictionary. The
// operations include navigation in the JSON data structure, as well as
// comparing the current (leaf) node to fixed values. The program also has a
// separate dictionary as working memory, into which it can memorize data, then
// later recall it for comparisons.
//
// Example program:
// NAVIGATE_ANY
// NAVIGATE(hash("bar"))
// COMPARE_NODE_BOOL(1)
// STORE_BOOL(hash("found_foo"), 1)
// STOP_EXECUTING_SENTENCE
//
// Example input:
// {
//   'key1': 1,
//   'key2': {'foo': 0, 'bar': false, 'baz': 2}
//   'key3': {'foo': 0, 'bar': true, 'baz': 2}
//   'key4': {'foo': 0, 'bar': true, 'baz': 2}
// }
//
// This program navigates from the root of the dictionary to all children
// ("key1", "key2", "key3", "key4") and executes the remaining program on each
// of the children. The navigation happens in depth-first pre-order. On each of
// the children it tries to navigate into the child "bar", which fails for
// "key1", so execution stops for this sub-branch. On key2 the program navigates
// to "bar" and moves the execution context to this node which has the value
// "false". Therefore, the following COMPARE_NODE_BOOL is not fulfilled and the
// execution does not continue on this branch, so we back track and proceed with
// "key3" and its "bar" child. For this node, COMPARE_NODE_BOOL is fulfilled and
// the execution continues to store "found_foo = true" into the working memory
// of the interpreter. Next the interpreter executes STOP_EXECUTING_SENTENCE
// which prevents the traversal from descending into the "key4" branch from the
// NAVIGATE_ANY operation and can therefore speedup the processing.
//
// All node names, and node values of type string, integer and double are hashed
// before being compared to hash values listed in |program|.

// JTL byte code consists of uint8 opcodes followed by parameters. Parameters
// are either boolean (uint8 with value \x00 or \x01), uint32 (in little-endian
// notation), or hash string of 32 bytes.
// The following opcodes are defined:
enum OpCodes {
  // Continues execution with the next operation on the element of a
  // dictionary that matches the passed key parameter. If no such element
  // exists, the command execution returns from the current instruction.
  // Parameters:
  // - a 32 byte hash of the target dictionary key.
  NAVIGATE = 0x00,
  // Continues execution with the next operation on each element of a
  // dictionary or list. If no such element exists or the current element is
  // neither a dictionary or list, the command execution returns from the
  // current instruction.
  NAVIGATE_ANY = 0x01,
  // Continues execution with the next operation on the parent node of the
  // current node. If the current node is the root of the input dictionary, the
  // program execution fails with a runtime error.
  NAVIGATE_BACK = 0x02,
  // Stores a boolean value in the working memory.
  // Parameters:
  // - a 32 byte hash of the parameter name,
  // - the value to store (\x00 or \x01)
  STORE_BOOL = 0x10,
  // Checks whether a boolean stored in working memory matches the expected
  // value and continues execution with the next operation in case of a match.
  // Parameters:
  // - a 32 byte hash of the parameter name,
  // - the expected value (\x00 or \x01),
  // - the default value in case the working memory contains no corresponding
  //   entry (\x00 or\x01).
  COMPARE_STORED_BOOL = 0x11,
  // Same as STORE_BOOL but takes a hash instead of a boolean value as
  // parameter.
  STORE_HASH = 0x12,
  // Same as COMPARE_STORED_BOOL but takes a hash instead of two boolean values
  // as parameters.
  COMPARE_STORED_HASH = 0x13,
  // Stores the current node into the working memory. If the current node is not
  // a boolean value, the program execution returns from the current
  // instruction.
  // Parameters:
  // - a 32 byte hash of the parameter name.
  STORE_NODE_BOOL = 0x14,
  // Stores the hashed value of the current node into the working memory. If
  // the current node is not a string, integer or double, the program execution
  // returns from the current instruction.
  // Parameters:
  // - a 32 byte hash of the parameter name.
  STORE_NODE_HASH = 0x15,
  // Parses the value of the current node as a URL, extracts the subcomponent of
  // the domain name that is immediately below the registrar-controlled portion,
  // and stores the hash of this subcomponent into working memory. In case the
  // domain name ends in a suffix not on the Public Suffix List (i.e. is neither
  // an ICANN, nor a private domain suffix), the last subcomponent is assumed to
  // be the registry, and the second-to-last subcomponent is extracted.
  // If the current node is not a string that represents a URL, or if this URL
  // does not have a domain component as described above, the program execution
  // returns from the current instruction.
  // For example, "foo.com", "www.foo.co.uk", "foo.appspot.com", and "foo.bar"
  // will all yield (the hash of) "foo" as a result.
  // See the unit test for more details.
  // Parameters:
  // - a 32 byte hash of the parameter name.
  STORE_NODE_REGISTERABLE_DOMAIN_HASH = 0x16,
  // Compares the current node against a boolean value and continues execution
  // with the next operation in case of a match. If the current node does not
  // match or is not a boolean value, the program execution returns from the
  // current instruction.
  // Parameters:
  // - a boolean value (\x00 or \x01).
  COMPARE_NODE_BOOL = 0x20,
  // Compares the hashed value of the current node against the given hash, and
  // continues execution with the next operation in case of a match. If the
  // current node is not a string, integer or double, or if it is either, but
  // its hash does not match, the program execution returns from the current
  // instruction.
  // Parameters:
  // - a 32 byte hash of the expected value.
  COMPARE_NODE_HASH = 0x21,
  // The negation of the above.
  COMPARE_NODE_HASH_NOT = 0x22,
  // Compares the current node against a boolean value stored in working memory,
  // and continues with the next operation in case of a match. If the current
  // node is not a boolean value, the working memory contains no corresponding
  // boolean value, or if they do not match, the program execution returns from
  // the current instruction.
  // Parameters:
  // - a 32 byte hash of the parameter name.
  COMPARE_NODE_TO_STORED_BOOL = 0x23,
  // Compares the hashed value of the current node against a hash value stored
  // in working memory, and continues with the next operation in case of a
  // match. If the current node is not a string, integer or double, or if the
  // working memory contains no corresponding hash string, or if the hashes do
  // not match, the program execution returns from the current instruction.
  // Parameters:
  // - a 32 byte hash of the parameter name.
  COMPARE_NODE_TO_STORED_HASH = 0x24,
  // Checks whether the current node is a string value that contains the given
  // pattern as a substring, and continues execution with the next operation in
  // case it does. If the current node is not a string, or if does not match the
  // pattern, the program execution returns from the current instruction.
  // Parameters:
  // - a 32 byte hash of the pattern,
  // - a 4 byte unsigned integer: the length (>0) of the pattern in bytes,
  // - a 4 byte unsigned integer: the sum of all bytes in the pattern, serving
  //   as a heuristic to reduce the number of actual SHA-256 calculations.
  COMPARE_NODE_SUBSTRING = 0x25,
  // Stop execution in this specific sentence.
  STOP_EXECUTING_SENTENCE = 0x30,
  // Separator between sentences, starts a new sentence.
  END_OF_SENTENCE = 0x31
};

const size_t kHashSizeInBytes = 32u;

// A class that provides SHA256 hash values for strings using a fixed hash seed.
class Hasher {
 public:
  explicit Hasher(const std::string& seed);
  ~Hasher();

  std::string GetHash(const std::string& input) const;

  static bool IsHash(const std::string& maybe_hash);

 private:
  crypto::HMAC hmac_;
  mutable std::map<std::string, std::string> cached_hashes_;
  DISALLOW_COPY_AND_ASSIGN(Hasher);
};

}  // namespace jtl_foundation

#endif  // CHROME_BROWSER_PROFILE_RESETTER_JTL_FOUNDATION_H_
