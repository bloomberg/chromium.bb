// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/jtl_interpreter.h"

#include "base/memory/scoped_vector.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/profile_resetter/jtl_foundation.h"
#include "crypto/hmac.h"

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
  explicit NavigateOperation(const std::string hashed_key)
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
  StoreValue(const std::string hashed_name, scoped_ptr<Value> value)
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
  CompareStoredValue(const std::string hashed_name,
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
    std::string actual_value;
    int tmp_int = 0;
    double tmp_double = 0.0;
    if (context->current_node()->GetAsInteger(&tmp_int)) {
      actual_value = base::IntToString(tmp_int);
    } else if (context->current_node()->GetAsDouble(&tmp_double)) {
      actual_value = base::DoubleToString(tmp_double);
    } else {
      if (!context->current_node()->GetAsString(&actual_value))
        return true;
    }
    actual_value = context->GetHash(actual_value);
    if (actual_value != hashed_value_)
      return true;
    return context->ContinueExecution();
  }

 private:
  std::string hashed_value_;
  DISALLOW_COPY_AND_ASSIGN(CompareNodeHash);
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
    if (next_instruction_index_ + 1u > program_.size())
      return false;
    *out = static_cast<uint8>(program_[next_instruction_index_]);
    ++next_instruction_index_;
    return true;
  }

  // Reads an operator code and returns whether this operation was successful.
  bool ReadOpCode(uint8* out) { return ReadUint8(out); }

  bool ReadHash(std::string* out) {
    if (next_instruction_index_ + jtl_foundation::kHashSizeInBytes >
        program_.size()) {
      return false;
    }
    char buffer[jtl_foundation::kHashSizeInBytes];
    for (size_t i = 0; i < arraysize(buffer); ++i) {
      buffer[i] = program_[next_instruction_index_];
      ++next_instruction_index_;
    }
    *out = std::string(buffer, arraysize(buffer));
    return true;
  }

  bool ReadBool(bool* out) {
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
