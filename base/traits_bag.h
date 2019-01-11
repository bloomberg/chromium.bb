// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRAITS_BAG_H_
#define BASE_TRAITS_BAG_H_

#include <initializer_list>
#include <tuple>
#include <type_traits>
#include <utility>

// A bag of Traits (structs / enums / etc...) can be an elegant alternative to
// the builder pattern and multiple default arguments for configuring things.
// Traits are terser than the builder pattern and can be evaluated at compile
// time, however they require the use of variadic templates which complicates
// matters. This file contains helpers that make Traits easier to use.
//
// E.g.
//   struct EnableFeatureX {};
//   struct UnusedTrait {};
//   enum Color { RED, BLUE };
//
//   struct ValidTraits {
//      ValidTraits(EnableFeatureX);
//      ValidTraits(Color);
//   };
//   ...
//   DoSomethingAwesome();                 // Use defaults (Color::BLUE &
//                                         // feature X not enabled)
//   DoSomethingAwesome(EnableFeatureX(),  // Turn feature X on
//                      Color::RED);       // And make it red.
//   DoSomethingAwesome(UnusedTrait(),     // Compile time error.
//                      Color::RED);
//
// DoSomethingAwesome might be defined as:
//
//   template <class... ArgTypes,
//             class CheckArgumentsAreValid = std::enable_if_t<
//                 trait_helpers::AreValidTraits<ValidTraits,
//                                               ArgTypes...>::value>>
//   constexpr void DoSomethingAwesome(ArgTypes... args)
//      : enable_feature_x(trait_helpers::HasTrait<EnableFeatureX>(args...)),
//        color(trait_helpers::GetEnum<Color, EnumTraitA::BLUE>(args...)) {}

namespace base {
namespace trait_helpers {

// Checks if any of the elements in |ilist| is true.
// Similar to std::any_of for the case of constexpr initializer_list.
inline constexpr bool any_of(std::initializer_list<bool> ilist) {
  for (auto c : ilist) {
    if (c)
      return true;
  }
  return false;
}

// Checks if all of the elements in |ilist| are true.
// Similar to std::any_of for the case of constexpr initializer_list.
inline constexpr bool all_of(std::initializer_list<bool> ilist) {
  for (auto c : ilist) {
    if (!c)
      return false;
  }
  return true;
}

// Counts the elements in |ilist| that are equal to |value|.
// Similar to std::count for the case of constexpr initializer_list.
template <class T>
inline constexpr size_t count(std::initializer_list<T> ilist, T value) {
  size_t c = 0;
  for (const auto& v : ilist) {
    c += (v == value);
  }
  return c;
}

// CallFirstTag is an argument tag that helps to avoid ambiguous overloaded
// functions. When the following call is made:
//    func(CallFirstTag(), arg...);
// the compiler will give precedence to an overload candidate that directly
// takes CallFirstTag. Another overload that takes CallSecondTag will be
// considered iff the preferred overload candidates were all invalids and
// therefore discarded.
struct CallSecondTag {};
struct CallFirstTag : CallSecondTag {};

// A trait filter class |TraitFilterType| implements the protocol to get a value
// of type |ArgType| from an argument list and convert it to a value of type
// |TraitType|. If the argument list contains an argument of type |ArgType|, the
// filter class will be instantiated with that argument. If the argument list
// contains no argument of type |ArgType|, the filter class will be instantiated
// using the default constructor if available; a compile error is issued
// otherwise. The filter class must have the conversion operator TraitType()
// which returns a value of type TraitType.

// |InvalidTrait| is used to return from GetTraitFromArg when the argument is
// not compatible with the desired trait.
struct InvalidTrait {};

// Returns an object of type |TraitFilterType| constructed from |arg| if
// compatible, or |InvalidTrait| otherwise.
template <class TraitFilterType,
          class ArgType,
          class CheckArgumentIsCompatible = std::enable_if_t<
              std::is_constructible<TraitFilterType, ArgType>::value>>
constexpr TraitFilterType GetTraitFromArg(CallFirstTag, ArgType arg) {
  return TraitFilterType(arg);
}

template <class TraitFilterType, class ArgType>
constexpr InvalidTrait GetTraitFromArg(CallSecondTag, ArgType arg) {
  return InvalidTrait();
}

// Returns an object of type |TraitFilterType| constructed from a compatible
// argument in |args...|, or default constructed if none of the arguments are
// compatible. This is the implementation of GetTraitFromArgList() with a
// disambiguation tag.
template <class TraitFilterType,
          class... ArgTypes,
          class TestCompatibleArgument = std::enable_if_t<any_of(
              {std::is_constructible<TraitFilterType, ArgTypes>::value...})>>
constexpr TraitFilterType GetTraitFromArgListImpl(CallFirstTag,
                                                  ArgTypes... args) {
  return std::get<TraitFilterType>(std::make_tuple(
      GetTraitFromArg<TraitFilterType>(CallFirstTag(), args)...));
}

template <class TraitFilterType, class... ArgTypes>
constexpr TraitFilterType GetTraitFromArgListImpl(CallSecondTag,
                                                  ArgTypes... args) {
  static_assert(std::is_constructible<TraitFilterType>::value,
                "The traits bag is missing a required trait.");
  return TraitFilterType();
}

// Constructs an object of type |TraitFilterType| from a compatible argument in
// |args...|, or using the default constructor, and returns its associated trait
// value using conversion to |TraitFilterType::ValueType|. If there are more
// than one compatible argument in |args|, generates a compile-time error.
template <class TraitFilterType, class... ArgTypes>
constexpr typename TraitFilterType::ValueType GetTraitFromArgList(
    ArgTypes... args) {
  static_assert(
      count({std::is_constructible<TraitFilterType, ArgTypes>::value...},
            true) <= 1,
      "The traits bag contains multiple traits of the same type.");
  return GetTraitFromArgListImpl<TraitFilterType>(CallFirstTag(), args...);
}

// Helper class to implemnent a |TraitFilterType|.
template <typename T>
struct BasicTraitFilter {
  using ValueType = T;

  constexpr operator ValueType() const { return value; }

  ValueType value = {};
};

template <typename ArgType>
struct BooleanTraitFilter : public BasicTraitFilter<bool> {
  constexpr BooleanTraitFilter() { this->value = false; }
  constexpr BooleanTraitFilter(ArgType) { this->value = true; }
};

template <typename ArgType, ArgType DefaultValue>
struct EnumTraitFilter : public BasicTraitFilter<ArgType> {
  constexpr EnumTraitFilter() { this->value = DefaultValue; }
  constexpr EnumTraitFilter(ArgType arg) { this->value = arg; }
};

// Tests whether multiple given argtument types are all valid traits according
// to the provided ValidTraits. To use, define a ValidTraits
template <typename ArgType>
struct RequiredEnumTraitFilter : public BasicTraitFilter<ArgType> {
  constexpr RequiredEnumTraitFilter(ArgType arg) { this->value = arg; }
};

// Tests whether a given trait type is valid or invalid by testing whether it is
// convertible to the provided ValidTraits type. To use, define a ValidTraits
// type like this:
//
// struct ValidTraits {
//   ValidTraits(MyTrait);
//   ...
// };
//
// You can 'inherit' valid traits like so:
//
// struct MoreValidTraits {
//   MoreValidTraits(ValidTraits);  // Pull in traits from ValidTraits.
//   MoreValidTraits(MyOtherTrait);
//   ...
// };
template <class ValidTraits, class... ArgTypes>
struct AreValidTraits
    : std::integral_constant<
          bool,
          all_of({std::is_constructible<ValidTraits, ArgTypes>::value...})> {};

// Helper to make getting an enum from a trait more readable.
template <typename Enum, typename... Args>
static constexpr Enum GetEnum(Args... args) {
  return GetTraitFromArgList<RequiredEnumTraitFilter<Enum>>(args...);
}

// Helper to make getting an enum from a trait with a default more readable.
template <typename Enum, Enum DefaultValue, typename... Args>
static constexpr Enum GetEnum(Args... args) {
  return GetTraitFromArgList<EnumTraitFilter<Enum, DefaultValue>>(args...);
}

// Helper to make checking for the presence of a trait more readable.
template <typename Trait, typename... Args>
static constexpr bool HasTrait(Args... args) {
  return GetTraitFromArgList<BooleanTraitFilter<Trait>>(args...);
}

// If you need a template vararg constructor to delegate to a private
// constructor, you may need to add this to the private constructor to ensure
// it's not matched by accident.
struct NotATraitTag {};

}  // namespace trait_helpers
}  // namespace base

#endif  // BASE_TRAITS_BAG_H_
