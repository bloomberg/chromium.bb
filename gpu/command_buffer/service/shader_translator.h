// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHADER_TRANSLATOR_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHADER_TRANSLATOR_H_

#include <string>

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "gpu/gpu_export.h"
#include "third_party/angle/include/GLSLANG/ShaderLang.h"

namespace gpu {
namespace gles2 {

// Translates a GLSL ES 2.0 shader to desktop GLSL shader, or just
// validates GLSL ES 2.0 shaders on a true GLSL ES implementation.
class ShaderTranslatorInterface {
 public:
  enum GlslImplementationType {
    kGlsl,
    kGlslES
  };

  enum GlslBuiltInFunctionBehavior {
    kGlslBuiltInFunctionOriginal,
    kGlslBuiltInFunctionEmulated
  };

  struct VariableInfo {
    VariableInfo()
        : type(0),
          size(0) {
    }

    VariableInfo(int _type, int _size, std::string _name)
        : type(_type),
          size(_size),
          name(_name) {
    }
    bool operator==(
        const ShaderTranslatorInterface::VariableInfo& other) const {
      return type == other.type &&
          size == other.size &&
          strcmp(name.c_str(), other.name.c_str()) == 0;
    }

    int type;
    int size;
    std::string name;  // name in the original shader source.
  };

  // Mapping between variable name and info.
  typedef base::hash_map<std::string, VariableInfo> VariableMap;
  // Mapping between hashed name and original name.
  typedef base::hash_map<std::string, std::string> NameMap;

  // Initializes the translator.
  // Must be called once before using the translator object.
  virtual bool Init(
      ShShaderType shader_type,
      ShShaderSpec shader_spec,
      const ShBuiltInResources* resources,
      GlslImplementationType glsl_implementation_type,
      GlslBuiltInFunctionBehavior glsl_built_in_function_behavior) = 0;

  // Translates the given shader source.
  // Returns true if translation is successful, false otherwise.
  virtual bool Translate(const char* shader) = 0;

  // The following functions return results from the last translation.
  // The results are NULL/empty if the translation was unsuccessful.
  // A valid info-log is always returned irrespective of whether translation
  // was successful or not.
  virtual const char* translated_shader() const = 0;
  virtual const char* info_log() const = 0;

  virtual const VariableMap& attrib_map() const = 0;
  virtual const VariableMap& uniform_map() const = 0;
  virtual const NameMap& name_map() const = 0;

 protected:
  virtual ~ShaderTranslatorInterface() {}
};

// Implementation of ShaderTranslatorInterface
class GPU_EXPORT ShaderTranslator
    : public base::RefCounted<ShaderTranslator>,
      NON_EXPORTED_BASE(public ShaderTranslatorInterface) {
 public:
  class DestructionObserver {
   public:
    DestructionObserver();
    virtual ~DestructionObserver();

    virtual void OnDestruct(ShaderTranslator* translator) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(DestructionObserver);
  };

  ShaderTranslator();

  // Overridden from ShaderTranslatorInterface.
  virtual bool Init(
      ShShaderType shader_type,
      ShShaderSpec shader_spec,
      const ShBuiltInResources* resources,
      GlslImplementationType glsl_implementation_type,
      GlslBuiltInFunctionBehavior glsl_built_in_function_behavior) OVERRIDE;

  // Overridden from ShaderTranslatorInterface.
  virtual bool Translate(const char* shader) OVERRIDE;

  // Overridden from ShaderTranslatorInterface.
  virtual const char* translated_shader() const OVERRIDE;
  virtual const char* info_log() const OVERRIDE;

  // Overridden from ShaderTranslatorInterface.
  virtual const VariableMap& attrib_map() const OVERRIDE;
  virtual const VariableMap& uniform_map() const OVERRIDE;
  virtual const NameMap& name_map() const OVERRIDE;

  void AddDestructionObserver(DestructionObserver* observer);
  void RemoveDestructionObserver(DestructionObserver* observer);

 private:
  friend class base::RefCounted<ShaderTranslator>;

  virtual ~ShaderTranslator();
  void ClearResults();

  ShHandle compiler_;
  scoped_array<char> translated_shader_;
  scoped_array<char> info_log_;
  VariableMap attrib_map_;
  VariableMap uniform_map_;
  NameMap name_map_;
  bool implementation_is_glsl_es_;
  bool needs_built_in_function_emulation_;
  ObserverList<DestructionObserver> destruction_observers_;

  DISALLOW_COPY_AND_ASSIGN(ShaderTranslator);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHADER_TRANSLATOR_H_

