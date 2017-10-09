// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/html_based_username_detector.h"

#include <algorithm>

#include "base/i18n/case_conversion.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/content/renderer/form_autofill_util.h"

using blink::WebFormControlElement;
using blink::WebInputElement;

namespace autofill {

namespace {

// For each input element that can be username, we compute and save developer
// and user group, along with associated short tokens lists (to handle finding
// less than |kMinimumWordLength| letters long words).
struct UsernameFieldData {
  WebInputElement input_element;
  base::string16 developer_value;
  std::vector<base::string16> developer_short_tokens;
  base::string16 user_value;
  std::vector<base::string16> user_short_tokens;
};

// "Latin" translations are the translations of the words for which the
// original translation is similar to the romanized translation (translation of
// the word only using ISO basic Latin alphabet).
// "Non-latin" translations are the translations of the words that have custom,
// country specific characters.
const char* const kNegativeLatin[] = {
    "pin",    "parola",   "wagwoord",   "wachtwoord",
    "fake",   "parole",   "givenname",  "achinsinsi",
    "token",  "parool",   "firstname",  "facalfaire",
    "fname",  "lozinka",  "pasahitza",  "focalfaire",
    "lname",  "passord",  "pasiwedhi",  "iphasiwedi",
    "geslo",  "huahuna",  "passwuert",  "katalaluan",
    "heslo",  "fullname", "phasewete",  "adgangskode",
    "parol",  "optional", "wachtwurd",  "contrasenya",
    "sandi",  "lastname", "cyfrinair",  "contrasinal",
    "senha",  "kupuhipa", "katasandi",  "kalmarsirri",
    "hidden", "password", "loluszais",  "tenimiafina",
    "second", "passwort", "middlename", "paroladordine",
    "codice", "pasvorto", "familyname", "inomboloyokuvula",
    "modpas", "salasana", "motdepasse", "numeraeleiloaesesi"};
constexpr int kNegativeLatinSize = arraysize(kNegativeLatin);

const char* const kNegativeNonLatin[] = {"fjalëkalim",
                                         "የይለፍቃል",
                                         "كلمهالسر",
                                         "գաղտնաբառ",
                                         "пароль",
                                         "পাসওয়ার্ড",
                                         "парола",
                                         "密码",
                                         "密碼",
                                         "დაგავიწყდათ",
                                         "κωδικόςπρόσβασης",
                                         "પાસવર્ડ",
                                         "סיסמה",
                                         "पासवर्ड",
                                         "jelszó",
                                         "lykilorð",
                                         "paswọọdụ",
                                         "パスワード",
                                         "ಪಾಸ್ವರ್ಡ್",
                                         "пароль",
                                         "ការពាក្យសម្ងាត់",
                                         "암호",
                                         "şîfre",
                                         "купуясөз",
                                         "ລະຫັດຜ່ານ",
                                         "slaptažodis",
                                         "лозинка",
                                         "पासवर्ड",
                                         "нууцүг",
                                         "စကားဝှက်ကို",
                                         "पासवर्ड",
                                         "رمز",
                                         "کلمهعبور",
                                         "hasło",
                                         "пароль",
                                         "лозинка",
                                         "پاسورڊ",
                                         "මුරපදය",
                                         "contraseña",
                                         "lösenord",
                                         "гузарвожа",
                                         "கடவுச்சொல்",
                                         "పాస్వర్డ్",
                                         "รหัสผ่าน",
                                         "пароль",
                                         "پاسورڈ",
                                         "mậtkhẩu",
                                         "פּאַראָל",
                                         "ọrọigbaniwọle"};
constexpr int kNegativeNonLatinSize = arraysize(kNegativeNonLatin);

const char* const kUsernameLatin[] = {
    "gatti",      "uzantonomo",   "solonanarana",    "nombredeusuario",
    "olumulo",    "nomenusoris",  "enwdefnyddiwr",   "nomdutilisateur",
    "lolowera",   "notandanafn",  "nomedeusuario",   "vartotojovardas",
    "username",   "ahanjirimara", "gebruikersnaam",  "numedeutilizator",
    "brugernavn", "benotzernumm", "jinalamtumiaji",  "erabiltzaileizena",
    "brukernavn", "benutzername", "sunanmaiamfani",  "foydalanuvchinomi",
    "mosebedisi", "kasutajanimi", "ainmcleachdaidh", "igamalomsebenzisi",
    "nomdusuari", "lomsebenzisi", "jenengpanganggo", "ingoakaiwhakamahi",
    "nomeutente", "namapengguna"};
constexpr int kUsernameLatinSize = arraysize(kUsernameLatin);

const char* const kUsernameNonLatin[] = {"用户名",
                                         "کاتيجونالو",
                                         "用戶名",
                                         "የተጠቃሚስም",
                                         "логин",
                                         "اسمالمستخدم",
                                         "נאמען",
                                         "کاصارفکانام",
                                         "ユーザ名",
                                         "όνομα χρήστη",
                                         "brûkersnamme",
                                         "корисничкоиме",
                                         "nonitilizatè",
                                         "корисничкоиме",
                                         "ngaranpamaké",
                                         "ຊື່ຜູ້ໃຊ້",
                                         "användarnamn",
                                         "యూజర్పేరు",
                                         "korisničkoime",
                                         "пайдаланушыаты",
                                         "שםמשתמש",
                                         "ім'якористувача",
                                         "کارننوم",
                                         "хэрэглэгчийннэр",
                                         "nomedeusuário",
                                         "имяпользователя",
                                         "têntruynhập",
                                         "பயனர்பெயர்",
                                         "ainmúsáideora",
                                         "ชื่อผู้ใช้",
                                         "사용자이름",
                                         "імякарыстальніка",
                                         "lietotājvārds",
                                         "потребителскоиме",
                                         "uporabniškoime",
                                         "колдонуучунунаты",
                                         "kullanıcıadı",
                                         "පරිශීලකනාමය",
                                         "istifadəçiadı",
                                         "օգտագործողիանունը",
                                         "navêbikarhêner",
                                         "ಬಳಕೆದಾರಹೆಸರು",
                                         "emriipërdoruesit",
                                         "वापरकर्तानाव",
                                         "käyttäjätunnus",
                                         "વપરાશકર્તાનામ",
                                         "felhasználónév",
                                         "उपयोगकर्तानाम",
                                         "nazwaużytkownika",
                                         "ഉപയോക്തൃനാമം",
                                         "სახელი",
                                         "အသုံးပြုသူအမည်",
                                         "نامکاربری",
                                         "प्रयोगकर्तानाम",
                                         "uživatelskéjméno",
                                         "ব্যবহারকারীরনাম",
                                         "užívateľskémeno",
                                         "ឈ្មោះអ្នកប្រើប្រាស់"};
constexpr int kUsernameNonLatinSize = arraysize(kUsernameNonLatin);

const char* const kUserLatin[] = {
    "user",   "wosuta",   "gebruiker",  "utilizator",
    "usor",   "notandi",  "gumagamit",  "vartotojas",
    "fammi",  "olumulo",  "maiamfani",  "cleachdaidh",
    "utent",  "pemakai",  "mpampiasa",  "umsebenzisi",
    "bruger", "usuario",  "panganggo",  "utilisateur",
    "bruker", "benotzer", "uporabnik",  "doutilizador",
    "numake", "benutzer", "covneegsiv", "erabiltzaile",
    "usuari", "kasutaja", "defnyddiwr", "kaiwhakamahi",
    "utente", "korisnik", "mosebedisi", "foydalanuvchi",
    "uzanto", "pengguna", "mushandisi"};
constexpr int kUserLatinSize = arraysize(kUserLatin);

const char* const kUserNonLatin[] = {"用户",
                                     "użytkownik",
                                     "tagatafaʻaaogā",
                                     "دکارونکيعکس",
                                     "用戶",
                                     "užívateľ",
                                     "корисник",
                                     "карыстальнік",
                                     "brûker",
                                     "kullanıcı",
                                     "истифода",
                                     "អ្នកប្រើ",
                                     "ọrụ",
                                     "ተጠቃሚ",
                                     "באַניצער",
                                     "хэрэглэгчийн",
                                     "يوزر",
                                     "istifadəçi",
                                     "ຜູ້ໃຊ້",
                                     "пользователь",
                                     "صارف",
                                     "meahoʻohana",
                                     "потребител",
                                     "वापरकर्ता",
                                     "uživatel",
                                     "ユーザー",
                                     "מִשׁתַמֵשׁ",
                                     "ผู้ใช้งาน",
                                     "사용자",
                                     "bikaranîvan",
                                     "колдонуучу",
                                     "વપરાશકર્તા",
                                     "përdorues",
                                     "ngườidùng",
                                     "корисникот",
                                     "उपयोगकर्ता",
                                     "itilizatè",
                                     "χρήστης",
                                     "користувач",
                                     "օգտվողիանձնագիրը",
                                     "használó",
                                     "faoiúsáideoir",
                                     "შესახებ",
                                     "ব্যবহারকারী",
                                     "lietotājs",
                                     "பயனர்",
                                     "ಬಳಕೆದಾರ",
                                     "ഉപയോക്താവ്",
                                     "کاربر",
                                     "యూజర్",
                                     "පරිශීලක",
                                     "प्रयोगकर्ता",
                                     "användare",
                                     "المستعمل",
                                     "пайдаланушы",
                                     "အသုံးပြုသူကို",
                                     "käyttäjä"};
constexpr int kUserNonLatinSize = arraysize(kUserNonLatin);

const char* const kTechnicalWords[] = {
    "uid",         "newtel",     "uaccount",   "regaccount",  "ureg",
    "loginid",     "laddress",   "accountreg", "regid",       "regname",
    "loginname",   "membername", "uname",      "ucreate",     "loginmail",
    "accountname", "umail",      "loginreg",   "accountid",   "loginaccount",
    "ulogin",      "regemail",   "newmobile",  "accountlogin"};
constexpr int kTechnicalWordsSize = arraysize(kTechnicalWords);

const char* const kWeakWords[] = {"id", "login", "mail"};
constexpr int kWeakWordsSize = arraysize(kWeakWords);

// Words that the algorithm looks for are split into multiple categories.
// A category may contain latin dictionary and non-latin dictionary. It is
// mandatory that it has latin one, but non-latin might be missing.
struct CategoryOfWords {
  const char* const* const latin_dictionary;
  const size_t latin_dictionary_size;
  const char* const* const non_latin_dictionary;
  const size_t non_latin_dictionary_size;
};

// Minimum length of a word, in order not to be considered short word.
// Short words will have different treatment than the others.
constexpr int kMinimumWordLength = 4;

void BuildValueAndShortTokens(
    const base::string16& raw_value,
    base::string16* field_data_value,
    std::vector<base::string16>* field_data_short_tokens) {
  // List of separators that can appear in HTML attribute values.
  static const std::string kDelimiters =
      "\"\'?%*@!\\/&^#:+~`;,>|<.[](){}-_ 0123456789";
  base::string16 lowercase_value = base::i18n::ToLower(raw_value);
  std::vector<base::StringPiece16> tokens =
      base::SplitStringPiece(lowercase_value, base::ASCIIToUTF16(kDelimiters),
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  *field_data_value = base::JoinString(tokens, base::string16());

  std::vector<base::StringPiece16> short_tokens;
  std::copy_if(tokens.begin(), tokens.end(), std::back_inserter(short_tokens),
               [](const base::StringPiece16& token) {
                 return token.size() < kMinimumWordLength;
               });

  for (const base::StringPiece16& token : short_tokens) {
    field_data_short_tokens->push_back(token.as_string());
  }
}

// For a given input element, compute developer and user value, along with
// developer and user short tokens.
UsernameFieldData ComputeFieldData(const blink::WebInputElement& input_element,
                                   const FormFieldData& field) {
  UsernameFieldData field_data;
  field_data.input_element = input_element;
  // When computing the developer value, '$' safety guard is being added
  // between field name and id, so that forming of accidental words is
  // prevented.
  BuildValueAndShortTokens(field.name + base::ASCIIToUTF16("$") + field.id,
                           &field_data.developer_value,
                           &field_data.developer_short_tokens);
  BuildValueAndShortTokens(field.label, &field_data.user_value,
                           &field_data.user_short_tokens);
  return field_data;
}

// For the fields of the given form that can be username fields, compute data
// needed by the detector.
void InferUsernameFieldData(
    const std::vector<blink::WebInputElement>& all_possible_usernames,
    const FormData& form_data,
    std::vector<UsernameFieldData>* possible_usernames_data) {
  // |all_possible_usernames| and |form_data.fields| may have different set of
  // fields. Match them based on |WebInputElement.NameForAutofill| and
  // |FormFieldData.name|.
  size_t current_index = 0;

  for (const blink::WebInputElement& input_element : all_possible_usernames) {
    for (size_t i = current_index; i < form_data.fields.size(); ++i) {
      const FormFieldData& field = form_data.fields[i];
      if (input_element.NameForAutofill().IsEmpty())
        continue;

      // Find matching form data and web input element.
      if (field.name == input_element.NameForAutofill().Utf16()) {
        current_index = i + 1;
        possible_usernames_data->push_back(
            ComputeFieldData(input_element, field));
        break;
      }
    }
  }
}

// Check if any word from the dictionary is encountered in computed field
// information.
bool SearchFieldInDictionary(const base::string16& value,
                             const std::vector<base::string16>& tokens,
                             const char* const* dictionary,
                             const size_t& dictionary_size) {
  for (size_t i = 0; i < dictionary_size; ++i) {
    if (strlen(dictionary[i]) < kMinimumWordLength) {
      // Treat short words by looking up for them in the tokens list.
      for (const base::string16& token : tokens) {
        if (token == base::UTF8ToUTF16(dictionary[i]))
          return true;
      }
    } else {
      // Treat long words by looking for them as a substring in |value|.
      if (value.find(base::UTF8ToUTF16(dictionary[i])) != std::string::npos)
        return true;
    }
  }
  return false;
}

// Check if any word from |category| is encountered in computed field
// information.
bool ContainsWordFromCategory(const UsernameFieldData& possible_username,
                              const CategoryOfWords& category) {
  // For user value, search in latin and non-latin dictionaries, because this
  // value is user visible.
  return SearchFieldInDictionary(
             possible_username.user_value, possible_username.user_short_tokens,
             category.latin_dictionary, category.latin_dictionary_size) ||
         SearchFieldInDictionary(possible_username.user_value,
                                 possible_username.user_short_tokens,
                                 category.non_latin_dictionary,
                                 category.non_latin_dictionary_size) ||
         // For developer value, only look up in latin dictionaries.
         SearchFieldInDictionary(possible_username.developer_value,
                                 possible_username.developer_short_tokens,
                                 category.latin_dictionary,
                                 category.latin_dictionary_size);
}

// Remove from |possible_usernames_data| the elements that definitely cannot be
// usernames, because their computed values contain at least one negative word.
void RemoveFieldsWithNegativeWords(
    std::vector<UsernameFieldData>* possible_usernames_data) {
  // Words that certainly point to a non-username field.
  // If field values contain at least one negative word, then the field is
  // excluded from the list of possible usernames.
  static const CategoryOfWords kNegativeCategory{
      kNegativeLatin, kNegativeLatinSize, kNegativeNonLatin,
      kNegativeNonLatinSize};

  possible_usernames_data->erase(
      std::remove_if(possible_usernames_data->begin(),
                     possible_usernames_data->end(),
                     [](const UsernameFieldData& possible_username) {
                       return ContainsWordFromCategory(possible_username,
                                                       kNegativeCategory);
                     }),
      possible_usernames_data->end());
}

// Check if any word from the given category appears in fields from the form.
// If a word appears in more than 2 fields, we do not make a decision, because
// it may just be a prefix.
// If a word appears in 1 or 2 fields, we return the first field in which we
// found the substring as |username_element|.
bool FormContainsWordFromCategory(
    const std::vector<UsernameFieldData>& possible_usernames_data,
    const CategoryOfWords& category,
    WebInputElement* username_element) {
  // Auxiliary element that contains the first field (in order of appearance in
  // the form) in which a substring is encountered.
  WebInputElement chosen_field;

  size_t count = 0;
  for (const UsernameFieldData& field_data : possible_usernames_data) {
    if (ContainsWordFromCategory(field_data, category)) {
      if (count == 0)
        chosen_field = field_data.input_element;
      count++;
    }
  }

  if (count && count <= 2) {
    *username_element = chosen_field;
    return true;
  }
  return false;
}

}  // namespace

bool GetUsernameFieldBasedOnHtmlAttributes(
    const std::vector<blink::WebInputElement>& all_possible_usernames,
    const FormData& form_data,
    WebInputElement* username_element) {
  DCHECK(username_element);

  // Translations of "username".
  static const CategoryOfWords kUsernameCategory{
      kUsernameLatin, kUsernameLatinSize, kUsernameNonLatin,
      kUsernameNonLatinSize};

  // Translations of "user".
  static const CategoryOfWords kUserCategory{kUserLatin, kUserLatinSize,
                                             kUserNonLatin, kUserNonLatinSize};

  // Words that certainly point to a username field, if they appear in developer
  // value. They are technical words, because they can only be used as variable
  // names, and not as stand-alone words.
  static const CategoryOfWords kTechnicalCategory{
      kTechnicalWords, kTechnicalWordsSize, nullptr, 0};

  // Words that might point to a username field.They have the smallest priority
  // in the heuristic, because there are also field attribute values that
  // contain them, but are not username fields.
  static const CategoryOfWords kWeakCategory{kWeakWords, kWeakWordsSize,
                                             nullptr, 0};

  // These categories contain words that point to username field.
  static const CategoryOfWords kPositiveCategories[] = {
      kUsernameCategory, kUserCategory, kTechnicalCategory, kWeakCategory};

  std::vector<UsernameFieldData> possible_usernames_data;
  InferUsernameFieldData(all_possible_usernames, form_data,
                         &possible_usernames_data);
  RemoveFieldsWithNegativeWords(&possible_usernames_data);

  // These are the searches performed by the username detector.
  // Order of categories is vital: the detector searches for words in descending
  // order of probability to point to a username field.
  for (const CategoryOfWords& category : kPositiveCategories) {
    if (FormContainsWordFromCategory(possible_usernames_data, category,
                                     username_element)) {
      return true;
    }
  }
  return false;
}

}  // namespace autofill
