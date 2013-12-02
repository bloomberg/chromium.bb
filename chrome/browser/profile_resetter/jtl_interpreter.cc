// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/jtl_interpreter.h"

#include <numeric>

#include "base/memory/scoped_vector.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/profile_resetter/jtl_foundation.h"
#include "crypto/hmac.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

namespace {

class ExecutionContext;

// An operation in an interpreted program.
class Operation {
 public:
  virtual ~Operation() {}
  // Executes the operation on the specified context and instructs the context
  // to continue execution with the next instruction if appropriate.
  // Returns true if we should continue with any potential backtracking that
  // needs to be done.
  virtual bool Execute(ExecutionContext* context) = 0;
};

// An execution context of operations.
class ExecutionContext {
 public:
  // |input| is the root of a dictionary that stores the information the
  // sentence is evaluated on.
  ExecutionContext(const jtl_foundation::Hasher* hasher,
                   const std::vector<Operation*>& sentence,
                   const DictionaryValue* input,
                   DictionaryValue* working_memory)
      : hasher_(hasher),
        sentence_(sentence),
        next_instruction_index_(0u),
        working_memory_(working_memory),
        error_(false) {
    stack_.push_back(input);
  }
  ~ExecutionContext() {}

  // Returns true in case of success.
  bool ContinueExecution() {
    if (error_ || stack_.empty()) {
      error_ = true;
      return false;
    }
    if (next_instruction_index_ >= sentence_.size())
      return true;

    Operation* op = sentence_[next_instruction_index_];
    next_instruction_index_++;
    bool continue_traversal = op->Execute(this);
    next_instruction_index_--;
    return continue_traversal;
  }

  std::string GetHash(const std::string& input) {
    return hasher_->GetHash(input);
  }

  // Calculates the |hash| of a string, integer or double |value|, and returns
  // true. Returns false otherwise.
  bool GetValueHash(const Value& value, std::string* hash) {
    DCHECK(hash);
    std::string value_as_string;
    int tmp_int = 0;
    double tmp_double = 0.0;
    if (value.GetAsInteger(&tmp_int))
      value_as_string = base::IntToString(tmp_int);
    else if (value.GetAsDouble(&tmp_double))
      value_as_string = base::DoubleToString(tmp_double);
    else if (!value.GetAsString(&value_as_string))
      return false;
    *hash = GetHash(value_as_string);
    return true;
  }

  const Value* current_node() const { return stack_.back(); }
  std::vector<const Value*>* stack() { return &stack_; }
  DictionaryValue* working_memory() { return working_memory_; }
  bool error() const { return error_; }

 private:
  // A hasher used to hash node names in a dictionary.
  const jtl_foundation::Hasher* hasher_;
  // The sentence to be executed.
  const std::vector<Operation*> sentence_;
  // Position in |sentence_|.
  size_t next_instruction_index_;
  // A stack of Values, indicating a navigation path from the root node of
  // |input| (see constructor) to the current node on which the
  // sentence_[next_instruction_index_] is evaluated.
  std::vector<const Value*> stack_;
  // Memory into which values can be stored by the program.
  DictionaryValue* working_memory_;
  // Whether a runtime error occurred.
  bool error_;
  DISALLOW_COPY_AND_ASSIGN(ExecutionContext);
};

class NavigateOperation : public Operation {
 public:
  explicit NavigateOperation(const std::string& hashed_key)
      : hashed_key_(hashed_key) {}
  virtual ~NavigateOperation() {}
  virtual bool Execute(ExecutionContext* context) OVERRIDE {
    const DictionaryValue* dict = NULL;
    if (!context->current_node()->GetAsDictionary(&dict)) {
      // Just ignore this node gracefully as this navigation is a dead end.
      // If this NavigateOperation occurred after a NavigateAny operation, those
      // may still be fulfillable, so we allow continuing the execution of the
      // sentence on other nodes.
      return true;
    }
    for (DictionaryValue::Iterator i(*dict); !i.IsAtEnd(); i.Advance()) {
      if (context->GetHash(i.key()) != hashed_key_)
        continue;
      context->stack()->push_back(&i.value());
      bool continue_traversal = context->ContinueExecution();
      context->stack()->pop_back();
      if (!continue_traversal)
        return false;
    }
    return true;
  }

 private:
  std::string hashed_key_;
  DISALLOW_COPY_AND_ASSIGN(NavigateOperation);
};

class NavigateAnyOperation : public Operation {
 public:
  NavigateAnyOperation() {}
  virtual ~NavigateAnyOperation() {}
  virtual bool Execute(ExecutionContext* context) OVERRIDE {
    const DictionaryValue* dict = NULL;
    const ListValue* list = NULL;
    if (context->current_node()->GetAsDictionary(&dict)) {
      for (DictionaryValue::Iterator i(*dict); !i.IsAtEnd(); i.Advance()) {
        context->stack()->push_back(&i.value());
        bool continue_traversal = context->ContinueExecution();
        context->stack()->pop_back();
        if (!continue_traversal)
          return false;
      }
    } else if (context->current_node()->GetAsList(&list)) {
      for (ListValue::const_iterator i = list->begin(); i != list->end(); ++i) {
        context->stack()->push_back(*i);
        bool continue_traversal = context->ContinueExecution();
        context->stack()->pop_back();
        if (!continue_traversal)
          return false;
      }
    } else {
      // Do nothing, just ignore this node.
    }
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NavigateAnyOperation);
};

class NavigateBackOperation : public Operation {
 public:
  NavigateBackOperation() {}
  virtual ~NavigateBackOperation() {}
  virtual bool Execute(ExecutionContext* context) OVERRIDE {
    const Value* current_node = context->current_node();
    context->stack()->pop_back();
    bool continue_traversal = context->ContinueExecution();
    context->stack()->push_back(current_node);
    return continue_traversal;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NavigateBackOperation);
};

class StoreValue : public Operation {
 public:
  StoreValue(const std::string& hashed_name, scoped_ptr<Value> value)
      : hashed_name_(hashed_name),
        value_(value.Pass()) {
    DCHECK(IsStringUTF8(hashed_name));
    DCHECK(value_);
  }
  virtual ~StoreValue() {}
  virtual bool Execute(ExecutionContext* context) OVERRIDE {
    context->working_memory()->Set(hashed_name_, value_->DeepCopy());
    return context->ContinueExecution();
  }

 private:
  std::string hashed_name_;
  scoped_ptr<Value> value_;
  DISALLOW_COPY_AND_ASSIGN(StoreValue);
};

class CompareStoredValue : public Operation {
 public:
  CompareStoredValue(const std::string& hashed_name,
                     scoped_ptr<Value> value,
                     scoped_ptr<Value> default_value)
      : hashed_name_(hashed_name),
        value_(value.Pass()),
        default_value_(default_value.Pass()) {
    DCHECK(IsStringUTF8(hashed_name));
    DCHECK(value_);
    DCHECK(default_value_);
  }
  virtual ~CompareStoredValue() {}
  virtual bool Execute(ExecutionContext* context) OVERRIDE {
    const Value* actual_value = NULL;
    if (!context->working_memory()->Get(hashed_name_, &actual_value))
      actual_value = default_value_.get();
    if (!value_->Equals(actual_value))
      return true;
    return context->ContinueExecution();
  }

 private:
  std::string hashed_name_;
  scoped_ptr<Value> value_;
  scoped_ptr<Value> default_value_;
  DISALLOW_COPY_AND_ASSIGN(CompareStoredValue);
};

template<bool ExpectedTypeIsBooleanNotHashable>
class StoreNodeValue : public Operation {
 public:
  explicit StoreNodeValue(const std::string& hashed_name)
      : hashed_name_(hashed_name) {
    DCHECK(IsStringUTF8(hashed_name));
  }
  virtual ~StoreNodeValue() {}
  virtual bool Execute(ExecutionContext* context) OVERRIDE {
    scoped_ptr<base::Value> value;
    if (ExpectedTypeIsBooleanNotHashable) {
      if (!context->current_node()->IsType(base::Value::TYPE_BOOLEAN))
        return true;
      value.reset(context->current_node()->DeepCopy());
    } else {
      std::string hash;
      if (!context->GetValueHash(*context->current_node(), &hash))
        return true;
      value.reset(new base::StringValue(hash));
    }
    context->working_memory()->Set(hashed_name_, value.release());
    return context->ContinueExecution();
  }

 private:
  std::string hashed_name_;
  DISALLOW_COPY_AND_ASSIGN(StoreNodeValue);
};

// Stores the effective SLD (second-level domain) of the URL represented by the
// current node into working memory.
class StoreNodeEffectiveSLD : public Operation {
 public:
  explicit StoreNodeEffectiveSLD(const std::string& hashed_name)
      : hashed_name_(hashed_name) {
    DCHECK(IsStringUTF8(hashed_name));
  }
  virtual ~StoreNodeEffectiveSLD() {}
  virtual bool Execute(ExecutionContext* context) OVERRIDE {
    std::string possibly_invalid_url;
    std::string effective_sld;
    if (!context->current_node()->GetAsString(&possibly_invalid_url) ||
        !GetEffectiveSLD(possibly_invalid_url, &effective_sld))
      return true;
    context->working_memory()->Set(
        hashed_name_, new StringValue(context->GetHash(effective_sld)));
    return context->ContinueExecution();
  }

 private:
  // If |possibly_invalid_url| is a valid URL that has an effective second-level
  // domain part, outputs that in |effective_sld| and returns true.
  // Returns false otherwise.
  static bool GetEffectiveSLD(const std::string& possibly_invalid_url,
                              std::string* effective_sld) {
    namespace domains = net::registry_controlled_domains;
    DCHECK(effective_sld);
    GURL url(possibly_invalid_url);
    if (!url.is_valid())
      return false;
    std::string sld_and_registry = domains::GetDomainAndRegistry(
        url.host(), domains::EXCLUDE_PRIVATE_REGISTRIES);
    size_t registry_length = domains::GetRegistryLength(
        url.host(),
        domains::EXCLUDE_UNKNOWN_REGISTRIES,
        domains::EXCLUDE_PRIVATE_REGISTRIES);
    // Fail unless (1.) the URL has a host part; and (2.) that host part is a
    // well-formed domain name that ends in, but is not in itself, as a whole,
    // a recognized registry identifier that is acknowledged by ICANN.
    if (registry_length == std::string::npos || registry_length == 0)
      return false;
    DCHECK_LT(registry_length, sld_and_registry.size());
    // Subtract one to cut off the dot separating the SLD and the registry.
    effective_sld->assign(
        sld_and_registry, 0, sld_and_registry.size() - registry_length - 1);
    return true;
  }

  std::string hashed_name_;
  DISALLOW_COPY_AND_ASSIGN(StoreNodeEffectiveSLD);
};

class CompareNodeBool : public Operation {
 public:
  explicit CompareNodeBool(bool value) : value_(value) {}
  virtual ~CompareNodeBool() {}
  virtual bool Execute(ExecutionContext* context) OVERRIDE {
    bool actual_value = false;
    if (!context->current_node()->GetAsBoolean(&actual_value))
      return true;
    if (actual_value != value_)
      return true;
    return context->ContinueExecution();
  }

 private:
  bool value_;
  DISALLOW_COPY_AND_ASSIGN(CompareNodeBool);
};

class CompareNodeHash : public Operation {
 public:
  explicit CompareNodeHash(const std::string& hashed_value)
      : hashed_value_(hashed_value) {}
  virtual ~CompareNodeHash() {}
  virtual bool Execute(ExecutionContext* context) OVERRIDE {
    std::string actual_hash;
    if (!context->GetValueHash(*context->current_node(), &actual_hash) ||
        actual_hash != hashed_value_)
      return true;
    return context->ContinueExecution();
  }

 private:
  std::string hashed_value_;
  DISALLOW_COPY_AND_ASSIGN(CompareNodeHash);
};

class CompareNodeHashNot : public Operation {
 public:
  explicit CompareNodeHashNot(const std::string& hashed_value)
      : hashed_value_(hashed_value) {}
  virtual ~CompareNodeHashNot() {}
  virtual bool Execute(ExecutionContext* context) OVERRIDE {
    std::string actual_hash;
    if (context->GetValueHash(*context->current_node(), &actual_hash) &&
        actual_hash == hashed_value_)
      return true;
    return context->ContinueExecution();
  }

 private:
  std::string hashed_value_;
  DISALLOW_COPY_AND_ASSIGN(CompareNodeHashNot);
};

template<bool ExpectedTypeIsBooleanNotHashable>
class CompareNodeToStored : public Operation {
 public:
  explicit CompareNodeToStored(const std::string& hashed_name)
      : hashed_name_(hashed_name) {}
  virtual ~CompareNodeToStored() {}
  virtual bool Execute(ExecutionContext* context) OVERRIDE {
    const Value* stored_value = NULL;
    if (!context->working_memory()->Get(hashed_name_, &stored_value))
      return true;
    if (ExpectedTypeIsBooleanNotHashable) {
      if (!context->current_node()->IsType(base::Value::TYPE_BOOLEAN) ||
          !context->current_node()->Equals(stored_value))
        return true;
    } else {
      std::string actual_hash;
      std::string stored_hash;
      if (!context->GetValueHash(*context->current_node(), &actual_hash) ||
          !stored_value->GetAsString(&stored_hash) ||
          actual_hash != stored_hash)
        return true;
    }
    return context->ContinueExecution();
  }

 private:
  std::string hashed_name_;
  DISALLOW_COPY_AND_ASSIGN(CompareNodeToStored);
};

class CompareNodeSubstring : public Operation {
 public:
  explicit CompareNodeSubstring(const std::string& hashed_pattern,
                                size_t pattern_length,
                                uint32 pattern_sum)
      : hashed_pattern_(hashed_pattern),
        pattern_length_(pattern_length),
        pattern_sum_(pattern_sum) {
    DCHECK(pattern_length_);
  }
  virtual ~CompareNodeSubstring() {}
  virtual bool Execute(ExecutionContext* context) OVERRIDE {
    std::string value_as_string;
    if (!context->current_node()->GetAsString(&value_as_string) ||
        !pattern_length_ || value_as_string.size() < pattern_length_)
      return true;
    // Go over the string with a sliding window. Meanwhile, maintain the sum in
    // an incremental fashion, and only calculate the SHA-256 hash when the sum
    // checks out so as to improve performance.
    std::string::const_iterator window_begin = value_as_string.begin();
    std::string::const_iterator window_end = window_begin + pattern_length_ - 1;
    uint32 window_sum =
        std::accumulate(window_begin, window_end, static_cast<uint32>(0u));
    while (window_end != value_as_string.end()) {
      window_sum += *window_end++;
      if (window_sum == pattern_sum_ && context->GetHash(std::string(
          window_begin, window_end)) == hashed_pattern_)
        return context->ContinueExecution();
      window_sum -= *window_begin++;
    }
    return true;
  }

 private:
  std::string hashed_pattern_;
  size_t pattern_length_;
  uint32 pattern_sum_;
  DISALLOW_COPY_AND_ASSIGN(CompareNodeSubstring);
};

class StopExecutingSentenceOperation : public Operation {
 public:
  StopExecutingSentenceOperation() {}
  virtual ~StopExecutingSentenceOperation() {}
  virtual bool Execute(ExecutionContext* context) OVERRIDE {
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(StopExecutingSentenceOperation);
};

class Parser {
 public:
  explicit Parser(const std::string& program)
      : program_(program),
        next_instruction_index_(0u) {}
  ~Parser() {}
  bool ParseNextSentence(ScopedVector<Operation>* output) {
    ScopedVector<Operation> operators;
    bool sentence_ended = false;
    while (next_instruction_index_ < program_.size() && !sentence_ended) {
      uint8 op_code = 0;
      if (!ReadOpCode(&op_code))
        return false;
      switch (static_cast<jtl_foundation::OpCodes>(op_code)) {
        case jtl_foundation::NAVIGATE: {
          std::string hashed_key;
          if (!ReadHash(&hashed_key))
            return false;
          operators.push_back(new NavigateOperation(hashed_key));
          break;
        }
        case jtl_foundation::NAVIGATE_ANY:
          operators.push_back(new NavigateAnyOperation);
          break;
        case jtl_foundation::NAVIGATE_BACK:
          operators.push_back(new NavigateBackOperation);
          break;
        case jtl_foundation::STORE_BOOL: {
          std::string hashed_name;
          if (!ReadHash(&hashed_name) || !IsStringUTF8(hashed_name))
            return false;
          bool value = false;
          if (!ReadBool(&value))
            return false;
          operators.push_back(new StoreValue(
              hashed_name,
              scoped_ptr<Value>(new base::FundamentalValue(value))));
          break;
        }
        case jtl_foundation::COMPARE_STORED_BOOL: {
          std::string hashed_name;
          if (!ReadHash(&hashed_name) || !IsStringUTF8(hashed_name))
            return false;
          bool value = false;
          if (!ReadBool(&value))
            return false;
          bool default_value = false;
          if (!ReadBool(&default_value))
            return false;
          operators.push_back(new CompareStoredValue(
              hashed_name,
              scoped_ptr<Value>(new base::FundamentalValue(value)),
              scoped_ptr<Value>(new base::FundamentalValue(default_value))));
          break;
        }
        case jtl_foundation::STORE_HASH: {
          std::string hashed_name;
          if (!ReadHash(&hashed_name) || !IsStringUTF8(hashed_name))
            return false;
          std::string hashed_value;
          if (!ReadHash(&hashed_value))
            return false;
          operators.push_back(new StoreValue(
              hashed_name,
              scoped_ptr<Value>(new base::StringValue(hashed_value))));
          break;
        }
        case jtl_foundation::COMPARE_STORED_HASH: {
          std::string hashed_name;
          if (!ReadHash(&hashed_name) || !IsStringUTF8(hashed_name))
            return false;
          std::string hashed_value;
          if (!ReadHash(&hashed_value))
            return false;
          std::string hashed_default_value;
          if (!ReadHash(&hashed_default_value))
            return false;
          operators.push_back(new CompareStoredValue(
              hashed_name,
              scoped_ptr<Value>(new base::StringValue(hashed_value)),
              scoped_ptr<Value>(new base::StringValue(hashed_default_value))));
          break;
        }
        case jtl_foundation::STORE_NODE_BOOL: {
          std::string hashed_name;
          if (!ReadHash(&hashed_name) || !IsStringUTF8(hashed_name))
            return false;
          operators.push_back(new StoreNodeValue<true>(hashed_name));
          break;
        }
        case jtl_foundation::STORE_NODE_HASH: {
          std::string hashed_name;
          if (!ReadHash(&hashed_name) || !IsStringUTF8(hashed_name))
            return false;
          operators.push_back(new StoreNodeValue<false>(hashed_name));
          break;
        }
        case jtl_foundation::STORE_NODE_EFFECTIVE_SLD_HASH: {
          std::string hashed_name;
          if (!ReadHash(&hashed_name) || !IsStringUTF8(hashed_name))
            return false;
          operators.push_back(new StoreNodeEffectiveSLD(hashed_name));
          break;
        }
        case jtl_foundation::COMPARE_NODE_BOOL: {
          bool value = false;
          if (!ReadBool(&value))
            return false;
          operators.push_back(new CompareNodeBool(value));
          break;
        }
        case jtl_foundation::COMPARE_NODE_HASH: {
          std::string hashed_value;
          if (!ReadHash(&hashed_value))
            return false;
          operators.push_back(new CompareNodeHash(hashed_value));
          break;
        }
        case jtl_foundation::COMPARE_NODE_HASH_NOT: {
          std::string hashed_value;
          if (!ReadHash(&hashed_value))
            return false;
          operators.push_back(new CompareNodeHashNot(hashed_value));
          break;
        }
        case jtl_foundation::COMPARE_NODE_TO_STORED_BOOL: {
          std::string hashed_name;
          if (!ReadHash(&hashed_name) || !IsStringUTF8(hashed_name))
            return false;
          operators.push_back(new CompareNodeToStored<true>(hashed_name));
          break;
        }
        case jtl_foundation::COMPARE_NODE_TO_STORED_HASH: {
          std::string hashed_name;
          if (!ReadHash(&hashed_name) || !IsStringUTF8(hashed_name))
            return false;
          operators.push_back(new CompareNodeToStored<false>(hashed_name));
          break;
        }
        case jtl_foundation::COMPARE_NODE_SUBSTRING: {
          std::string hashed_pattern;
          uint32 pattern_length = 0, pattern_sum = 0;
          if (!ReadHash(&hashed_pattern))
            return false;
          if (!ReadUint32(&pattern_length) || pattern_length == 0)
            return false;
          if (!ReadUint32(&pattern_sum))
            return false;
          operators.push_back(new CompareNodeSubstring(
              hashed_pattern, pattern_length, pattern_sum));
          break;
        }
        case jtl_foundation::STOP_EXECUTING_SENTENCE:
          operators.push_back(new StopExecutingSentenceOperation);
          break;
        case jtl_foundation::END_OF_SENTENCE:
          sentence_ended = true;
          break;
        default:
          return false;
      }
    }
    output->swap(operators);
    return true;
  }

  bool HasNextSentence() const {
    return next_instruction_index_ < program_.size();
  }

 private:
  // Reads an uint8 and returns whether this operation was successful.
  bool ReadUint8(uint8* out) {
    DCHECK(out);
    if (next_instruction_index_ + 1u > program_.size())
      return false;
    *out = static_cast<uint8>(program_[next_instruction_index_]);
    ++next_instruction_index_;
    return true;
  }

  // Reads an uint32 and returns whether this operation was successful.
  bool ReadUint32(uint32* out) {
    DCHECK(out);
    if (next_instruction_index_ + 4u > program_.size())
      return false;
    *out = 0u;
    for (int i = 0; i < 4; ++i) {
      *out >>= 8;
      *out |= static_cast<uint8>(program_[next_instruction_index_]) << 24;
      ++next_instruction_index_;
    }
    return true;
  }

  // Reads an operator code and returns whether this operation was successful.
  bool ReadOpCode(uint8* out) { return ReadUint8(out); }

  bool ReadHash(std::string* out) {
    DCHECK(out);
    if (next_instruction_index_ + jtl_foundation::kHashSizeInBytes >
        program_.size())
      return false;
    *out = program_.substr(next_instruction_index_,
                           jtl_foundation::kHashSizeInBytes);
    next_instruction_index_ += jtl_foundation::kHashSizeInBytes;
    DCHECK(jtl_foundation::Hasher::IsHash(*out));
    return true;
  }

  bool ReadBool(bool* out) {
    DCHECK(out);
    uint8 value = 0;
    if (!ReadUint8(&value))
      return false;
    if (value == 0)
      *out = false;
    else if (value == 1)
      *out = true;
    else
      return false;
    return true;
  }

  std::string program_;
  size_t next_instruction_index_;
  DISALLOW_COPY_AND_ASSIGN(Parser);
};

}  // namespace

JtlInterpreter::JtlInterpreter(
    const std::string& hasher_seed,
    const std::string& program,
    const DictionaryValue* input)
    : hasher_seed_(hasher_seed),
      program_(program),
      input_(input),
      working_memory_(new DictionaryValue),
      result_(OK) {
  DCHECK(input->IsType(Value::TYPE_DICTIONARY));
}

JtlInterpreter::~JtlInterpreter() {}

void JtlInterpreter::Execute() {
  jtl_foundation::Hasher hasher(hasher_seed_);
  Parser parser(program_);
  while (parser.HasNextSentence()) {
    ScopedVector<Operation> sentence;
    if (!parser.ParseNextSentence(&sentence)) {
      result_ = PARSE_ERROR;
      return;
    }
    ExecutionContext context(
        &hasher, sentence.get(), input_, working_memory_.get());
    context.ContinueExecution();
    if (context.error()) {
      result_ = RUNTIME_ERROR;
      return;
    }
  }
}

bool JtlInterpreter::GetOutputBoolean(const std::string& unhashed_key,
                                      bool* output) const {
  std::string hashed_key =
      jtl_foundation::Hasher(hasher_seed_).GetHash(unhashed_key);
  return working_memory_->GetBoolean(hashed_key, output);
}

bool JtlInterpreter::GetOutputString(const std::string& unhashed_key,
                                     std::string* output) const {
  std::string hashed_key =
      jtl_foundation::Hasher(hasher_seed_).GetHash(unhashed_key);
  return working_memory_->GetString(hashed_key, output);
}
