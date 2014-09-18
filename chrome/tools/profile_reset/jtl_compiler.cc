// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/tools/profile_reset/jtl_compiler.h"

#include <limits>
#include <map>
#include <numeric>

#include "base/logging.h"
#include "chrome/browser/profile_resetter/jtl_foundation.h"
#include "chrome/tools/profile_reset/jtl_parser.h"

namespace jtl = jtl_foundation;

namespace {

// Serializes symbols into byte-code in a streaming manner.
class ByteCodeWriter {
 public:
  explicit ByteCodeWriter(std::string* output) : output_(output) {}
  ~ByteCodeWriter() {}

  void WriteUint8(uint8 value) { output_->push_back(static_cast<char>(value)); }
  void WriteUint32(uint32 value) {
    for (int i = 0; i < 4; ++i) {
      output_->push_back(static_cast<char>(value & 0xFFu));
      value >>= 8;
    }
  }
  void WriteOpCode(uint8 op_code) { WriteUint8(op_code); }
  void WriteHash(const std::string& hash) {
    CHECK(jtl::Hasher::IsHash(hash));
    *output_ += hash;
  }
  void WriteBool(bool value) { WriteUint8(value ? 1u : 0u); }

 private:
  std::string* output_;

  DISALLOW_COPY_AND_ASSIGN(ByteCodeWriter);
};

// Encapsulates meta-data about all instructions, and is capable of transcoding
// each instruction from a parsed text-based format to byte-code.
class InstructionSet {
 public:
  InstructionSet() {
    // Define each instruction in this list.
    // Note:
    //  - Instructions ending in "hash" will write their 'HashString' arguments
    //    directly into the byte-code.
    //  - Instructions ending in "hashed" will first hash their 'String'
    //    arguments, and will write this hash to the byte-code.
    Add(Instruction("go", jtl::NAVIGATE, Arguments(String)));
    Add(Instruction("any", jtl::NAVIGATE_ANY, Arguments()));
    Add(Instruction("back", jtl::NAVIGATE_BACK, Arguments()));
    Add(Instruction("store_bool", jtl::STORE_BOOL, Arguments(String, Bool)));
    Add(Instruction("store_hash",
                    jtl::STORE_HASH, Arguments(String, HashString)));
    Add(Instruction("store_hashed",
                    jtl::STORE_HASH, Arguments(String, String)));
    Add(Instruction("store_node_bool",
                    jtl::STORE_NODE_BOOL, Arguments(String)));
    Add(Instruction("store_node_hash",
                    jtl::STORE_NODE_HASH, Arguments(String)));
    Add(Instruction("store_node_registerable_domain_hash",
                    jtl::STORE_NODE_REGISTERABLE_DOMAIN_HASH,
                    Arguments(String)));
    Add(Instruction("compare_bool", jtl::COMPARE_NODE_BOOL, Arguments(Bool)));
    Add(Instruction("compare_hashed",
                    jtl::COMPARE_NODE_HASH, Arguments(String)));
    Add(Instruction("compare_hashed_not",
                    jtl::COMPARE_NODE_HASH_NOT, Arguments(String)));
    Add(Instruction("compare_stored_bool",
                    jtl::COMPARE_STORED_BOOL,
                    Arguments(String, Bool, Bool)));
    Add(Instruction("compare_stored_hashed",
                    jtl::COMPARE_STORED_HASH,
                    Arguments(String, String, String)));
    Add(Instruction("compare_to_stored_bool",
                    jtl::COMPARE_NODE_TO_STORED_BOOL,
                    Arguments(String)));
    Add(Instruction("compare_to_stored_hash",
                    jtl::COMPARE_NODE_TO_STORED_HASH,
                    Arguments(String)));
    Add(Instruction("compare_substring_hashed",
                    jtl::COMPARE_NODE_SUBSTRING,
                    Arguments(StringPattern)));
    Add(Instruction("break", jtl::STOP_EXECUTING_SENTENCE, Arguments()));
  }

  JtlCompiler::CompileError::ErrorCode TranscodeInstruction(
      const std::string& name,
      const base::ListValue& arguments,
      bool ends_sentence,
      const jtl::Hasher& hasher,
      ByteCodeWriter* target) const {
    if (instruction_map_.count(name) == 0)
      return JtlCompiler::CompileError::INVALID_OPERATION_NAME;
    const Instruction& instruction(instruction_map_.at(name));
    if (instruction.argument_types.size() != arguments.GetSize())
      return JtlCompiler::CompileError::INVALID_ARGUMENT_COUNT;
    target->WriteOpCode(instruction.op_code);
    for (size_t i = 0; i < arguments.GetSize(); ++i) {
      switch (instruction.argument_types[i]) {
        case Bool: {
          bool value = false;
          if (!arguments.GetBoolean(i, &value))
            return JtlCompiler::CompileError::INVALID_ARGUMENT_TYPE;
          target->WriteBool(value);
          break;
        }
        case String: {
          std::string value;
          if (!arguments.GetString(i, &value))
            return JtlCompiler::CompileError::INVALID_ARGUMENT_TYPE;
          target->WriteHash(hasher.GetHash(value));
          break;
        }
        case StringPattern: {
          std::string value;
          if (!arguments.GetString(i, &value))
            return JtlCompiler::CompileError::INVALID_ARGUMENT_TYPE;
          if (value.empty() ||
              value.size() > std::numeric_limits<uint32>::max())
            return JtlCompiler::CompileError::INVALID_ARGUMENT_VALUE;
          target->WriteHash(hasher.GetHash(value));
          target->WriteUint32(static_cast<uint32>(value.size()));
          uint32 pattern_sum = std::accumulate(
              value.begin(), value.end(), static_cast<uint32>(0u));
          target->WriteUint32(pattern_sum);
          break;
        }
        case HashString: {
          std::string hash_value;
          if (!arguments.GetString(i, &hash_value) ||
              !jtl::Hasher::IsHash(hash_value))
            return JtlCompiler::CompileError::INVALID_ARGUMENT_TYPE;
          target->WriteHash(hash_value);
          break;
        }
        default:
          NOTREACHED();
          return JtlCompiler::CompileError::INVALID_ARGUMENT_TYPE;
      }
    }
    if (ends_sentence)
      target->WriteOpCode(jtl::END_OF_SENTENCE);
    return JtlCompiler::CompileError::ERROR_NONE;
  }

 private:
  // The possible types of an operation's argument.
  enum ArgumentType {
    None,
    Bool,
    String,
    StringPattern,
    HashString
  };

  // Encapsulates meta-data about one instruction.
  struct Instruction {
    Instruction() : op_code(jtl::END_OF_SENTENCE) {}
    Instruction(const char* name,
                jtl_foundation::OpCodes op_code,
                const std::vector<ArgumentType>& argument_types)
        : name(name), op_code(op_code), argument_types(argument_types) {}

    std::string name;
    jtl::OpCodes op_code;
    std::vector<ArgumentType> argument_types;
  };

  static std::vector<ArgumentType> Arguments(ArgumentType arg1_type = None,
                                             ArgumentType arg2_type = None,
                                             ArgumentType arg3_type = None) {
    std::vector<ArgumentType> result;
    if (arg1_type != None)
      result.push_back(arg1_type);
    if (arg2_type != None)
      result.push_back(arg2_type);
    if (arg3_type != None)
      result.push_back(arg3_type);
    return result;
  }

  void Add(const Instruction& instruction) {
    instruction_map_[instruction.name] = instruction;
  }

  std::map<std::string, Instruction> instruction_map_;

  DISALLOW_COPY_AND_ASSIGN(InstructionSet);
};

}  // namespace

bool JtlCompiler::Compile(const std::string& source_code,
                          const std::string& hash_seed,
                          std::string* output_bytecode,
                          CompileError* error_details) {
  DCHECK(output_bytecode);
  InstructionSet instruction_set;
  ByteCodeWriter bytecode_writer(output_bytecode);
  jtl::Hasher hasher(hash_seed);

  std::string compacted_source_code;
  std::vector<size_t> newline_indices;
  size_t mismatched_quotes_line;
  if (!JtlParser::RemoveCommentsAndAllWhitespace(source_code,
                                                 &compacted_source_code,
                                                 &newline_indices,
                                                 &mismatched_quotes_line)) {
    if (error_details) {
      error_details->context = "";  // No meaningful intra-line context here.
      error_details->line_number = mismatched_quotes_line;
      error_details->error_code = CompileError::MISMATCHED_DOUBLE_QUOTES;
    }
    return false;
  }

  JtlParser parser(compacted_source_code, newline_indices);
  while (!parser.HasFinished()) {
    std::string operation_name;
    base::ListValue arguments;
    bool ends_sentence = false;
    if (!parser.ParseNextOperation(
             &operation_name, &arguments, &ends_sentence)) {
      if (error_details) {
        error_details->context = parser.GetLastContext();
        error_details->line_number = parser.GetLastLineNumber();
        error_details->error_code = CompileError::PARSING_ERROR;
      }
      return false;
    }
    CompileError::ErrorCode error_code = instruction_set.TranscodeInstruction(
        operation_name, arguments, ends_sentence, hasher, &bytecode_writer);
    if (error_code != CompileError::ERROR_NONE) {
      if (error_details) {
        error_details->context = parser.GetLastContext();
        error_details->line_number = parser.GetLastLineNumber();
        error_details->error_code = error_code;
      }
      return false;
    }
  }

  return true;
}
